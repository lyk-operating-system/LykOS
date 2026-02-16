#include "mm/dma.h"
#include "mm/heap.h"
#include "mm/vm.h"
#include <stdint.h>
#define LOG_PREFIX "NVME"
#include "nvme.h"

#include "dev/storage/drive.h"
#include "log.h"
#include "dev/bus/pci.h"
#include "mm/mm.h"
#include "mm/mmio.h"
#include "arch/lcpu.h"
#include "sync/spinlock.h"
#include "utils/string.h"


#define NVME_CC_OFF   0x14
#define NVME_CSTS_OFF 0x1C

#define CC_EN          (1u << 0)
#define CC_CSS_SHIFT   4
#define CC_MPS_SHIFT   7
#define CC_AMS_SHIFT   11
#define CC_IOSQES_SHIFT 16
#define CC_IOCQES_SHIFT 20

// --- HELPERS ---

volatile nvme_cq_entry_t *nvme_poll_cq(nvme_t *nvme, nvme_queue_t *queue)
{
    (void)nvme;

    volatile nvme_cq_entry_t *entry = &queue->cq[queue->head];

    if (NVME_CQE_PHASE(entry) != queue->phase)
        return NULL;

    uint16_t cid = entry->cid;

    queue->head = (queue->head + 1) % queue->depth;
    if (queue->head == 0)
        queue->phase ^= 1;

    NVME_CQ_HDBL(nvme->registers, queue->qid, nvme->db_stride) = queue->head;

    spinlock_acquire(&queue->lock);
    if (cid < queue->depth) // minimal safety
        queue->cid_used[cid] = false;
    spinlock_release(&queue->lock);

    return entry;
}

static inline uint32_t nvme_read_reg(volatile void *base, uintptr_t off)
{
    return *(volatile uint32_t *)((uintptr_t)base + off);
}
static inline void nvme_write_reg(volatile void *base, uintptr_t off, uint32_t v)
{
    *(volatile uint32_t *)((uintptr_t)base + off) = v;
}

static void nvme_wait_ready(nvme_t *nvme, bool ready)
{
    nvme_cap_t cap = nvme->registers->CAP;
    uint32_t to = cap.to ? cap.to : 1;
    uint64_t timeout = (uint64_t)to * 5000000ull;

    while (nvme->registers->CSTS.rdy != ready && timeout--)
        arch_lcpu_relax();

    if (timeout == 0)
    {
        uint32_t csts = nvme_read_reg(nvme->registers, NVME_CSTS_OFF);
        uint32_t cc   = nvme_read_reg(nvme->registers, NVME_CC_OFF);
        log(LOG_ERROR, "wait_ready timeout want=%d CSTS=0x%08x CC=0x%08x", ready, csts, cc);
    }
}

// --- BASIC FUNCS ---

void nvme_reset(nvme_t *nvme)
{
    log(LOG_DEBUG, "Resetting...");

    // Spec wants: set EN=0 then wait RDY=0
    nvme->registers->CC.en = 0;
    nvme_wait_ready(nvme, false);
}

void nvme_start(nvme_t *nvme)
{
    uint32_t cc = 0;
    cc |= (0u << CC_CSS_SHIFT);      // NVM
    cc |= (0u << CC_MPS_SHIFT);      // 4KiB
    cc |= (0u << CC_AMS_SHIFT);      // round-robin
    cc |= (6u << CC_IOSQES_SHIFT);   // 64B
    cc |= (4u << CC_IOCQES_SHIFT);   // 16B
    cc |= CC_EN;

    nvme_write_reg(nvme->registers, NVME_CC_OFF, cc);
    (void)nvme_read_reg(nvme->registers, NVME_CC_OFF);

    nvme_wait_ready(nvme, true);
}

// --- ADMIN FUNCS ---

static void nvme_create_admin_queue(nvme_t *nvme)
{
    size_t sq_size = NVME_ADMIN_QUEUE_DEPTH * sizeof(nvme_sq_entry_t);
    size_t cq_size = NVME_ADMIN_QUEUE_DEPTH * sizeof(nvme_cq_entry_t);

    nvme_queue_t *aq = vm_alloc(sizeof(nvme_queue_t));
    nvme->admin_queue = aq;

    aq->sq_dma = dma_alloc(sq_size);
    aq->cq_dma = dma_alloc(cq_size);

    aq->sq = (nvme_sq_entry_t *)aq->sq_dma.vaddr;
    aq->cq = (volatile nvme_cq_entry_t *)aq->cq_dma.vaddr;

    memset(aq->sq_dma.vaddr, 0, sq_size);
    memset(aq->cq_dma.vaddr, 0, cq_size);

    aq->qid = 0;
    aq->depth = NVME_ADMIN_QUEUE_DEPTH;
    aq->head = 0;
    aq->tail = 0;
    aq->phase = 1;

    aq->next_cid = 0;
    memset(aq->cid_used, 0, sizeof(aq->cid_used));
    aq->lock = SPINLOCK_INIT;

    nvme->registers->AQA.asqs = NVME_ADMIN_QUEUE_DEPTH - 1;
    nvme->registers->AQA.acqs = NVME_ADMIN_QUEUE_DEPTH - 1;

    nvme->registers->ASQ = aq->sq_dma.paddr;
    nvme->registers->ACQ = aq->cq_dma.paddr;
}

// --- COMMAND FUNCTIONS ---

static uint16_t nvme_submit_command(nvme_t *nvme, uint8_t opc, nvme_command_t command, nvme_queue_t *q)
{
    // nvme_queue_t *q = nvme->admin_queue;

    spinlock_acquire(&q->lock);

    uint16_t cid = UINT16_MAX;
    for (uint16_t i = 0; i < q->depth; i++)
    {
        uint16_t try = (q->next_cid + i) % q->depth;
        if (!q->cid_used[try])
        {
            cid = try;
            q->cid_used[try] = true;
            q->next_cid = (try + 1) % q->depth;
            break;
        }
    }

    if (cid == UINT16_MAX)
    {
        spinlock_release(&q->lock);
        return UINT16_MAX;
    }

    uint16_t old_tail = q->tail;
    uint16_t next_tail = (q->tail + 1) % q->depth;
    if (next_tail == q->head)
    {
        q->cid_used[cid] = false;
        spinlock_release(&q->lock);
        return UINT16_MAX;
    }

    nvme_sq_entry_t e = {0};
    e.opc = opc;
    e.cid = cid;
    e.psdt = 0;
    e.command = command;

    q->sq[old_tail] = e;

    __atomic_thread_fence(__ATOMIC_SEQ_CST);

    q->tail = next_tail;
    NVME_SQ_TDBL(nvme->registers, q->qid, nvme->db_stride) = q->tail;

    spinlock_release(&q->lock);
    return cid;
}

static void nvme_wait_completion(nvme_t *nvme, uint16_t cid, nvme_queue_t *q)
{
    nvme_cap_t cap = nvme->registers->CAP;
    uint64_t timeout = (uint64_t)(cap.to ? cap.to : 1) * 5000000ull;

    while (timeout--)
    {
        volatile nvme_cq_entry_t *entry = nvme_poll_cq(nvme, q);
        if (entry && entry->cid == cid)
        {
            // Read raw DW3 of CQE (bytes 12..15). This avoids any bitfield/layout issues.
            uint32_t dw3 = *(volatile uint32_t *)((volatile uint8_t *)entry + 12);

            uint8_t  p   = (uint8_t)((dw3 >> 16) & 0x1u);
            uint8_t  sc  = (uint8_t)((dw3 >> 17) & 0xFFu);
            uint8_t  sct = (uint8_t)((dw3 >> 25) & 0x7u);
            uint8_t  dnr = (uint8_t)((dw3 >> 31) & 0x1u);

            if (sct != 0 || sc != 0)
                log(LOG_ERROR, "cid=%u failed: SCT=%u SC=0x%02x DNR=%u DW3=0x%08x P=%u",
                    cid, sct, sc, dnr, dw3, p);

            return;
        }
    }

    volatile nvme_cq_entry_t *e0 = &q->cq[q->head];

    // uh... keep all this debugging for now
    uint32_t dw3 = *(volatile uint32_t *)((volatile uint8_t *)e0 + 12);

    uint8_t  p   = (uint8_t)((dw3 >> 16) & 0x1u);
    uint8_t  sc  = (uint8_t)((dw3 >> 17) & 0xFFu);
    uint8_t  sct = (uint8_t)((dw3 >> 25) & 0x7u);
    uint8_t  dnr = (uint8_t)((dw3 >> 31) & 0x1u);

    log(LOG_ERROR,
        "cid=%u timed out. Queue head=%u tail=%u phase=%u CQ[head]: cid=%u DW3=0x%08x P=%u SCT=%u SC=0x%02x DNR=%u",
        cid, q->head, q->tail, q->phase, e0->cid, dw3, p, sct, sc, dnr);
}

// --- IO AND IRQ ---

static void nvme_create_io_queue(nvme_t *nvme)
{
    const uint16_t qid = 1; // TO-DO: change, hard-coded for now

    size_t sq_size = NVME_IO_QUEUE_DEPTH * sizeof(nvme_sq_entry_t);
    size_t cq_size = NVME_IO_QUEUE_DEPTH * sizeof(nvme_cq_entry_t);

    nvme_queue_t *ioq = vm_alloc(sizeof(nvme_queue_t));
    memset(ioq, 0, sizeof(nvme_queue_t));
    nvme->io_queue = ioq;

    ioq->sq_dma = dma_alloc(sq_size);
    ioq->cq_dma = dma_alloc(cq_size);

    ioq->sq = (nvme_sq_entry_t *)ioq->sq_dma.vaddr;
    ioq->cq = (volatile nvme_cq_entry_t *)ioq->cq_dma.vaddr;

    memset(ioq->sq_dma.vaddr, 0, sq_size);
    memset(ioq->cq_dma.vaddr, 0, cq_size);

    ioq->qid = qid;
    ioq->depth = NVME_IO_QUEUE_DEPTH;
    ioq->head = 0;
    ioq->tail = 0;
    ioq->phase = 1;
    ioq->next_cid = 0;
    memset(ioq->cid_used, 0, sizeof(ioq->cid_used));
    ioq->lock = SPINLOCK_INIT;

    // creating completion & submission queues
    /* source: https://nvmexpress.org/wp-content/uploads/NVM-Express-Base-Specification-Revision-2.3-2025.08.01-Ratified.pdf
       5.3.1 - 5.3.2 */
    // ------------

    // send admin command to create completion queue
    nvme_command_t cqc = (nvme_command_t){0};
    cqc.dptr.prp1 = ioq->cq_dma.paddr;
    cqc.cdw10 = ((uint32_t)qid) | ((uint32_t)(NVME_IO_QUEUE_DEPTH - 1) << 16);

    uint32_t iv = 0;
    uint32_t ien = 0;
    uint32_t pc = 1;
    cqc.cdw11 = (pc & 1u) | ((ien & 1u) << 1) | ((iv & 0xFFFFu) << 16);

    uint16_t cid = nvme_submit_command(nvme, 0x05, cqc, nvme->admin_queue);
    if (cid != UINT16_MAX) nvme_wait_completion(nvme, cid, nvme->admin_queue);

    // send admin command to create submission queue
    nvme_command_t sqc = (nvme_command_t){0};
    sqc.dptr.prp1 = ioq->sq_dma.paddr;
    sqc.cdw10 = ((uint32_t)qid) | ((uint32_t)(NVME_IO_QUEUE_DEPTH - 1) << 16);

    uint32_t cqid = qid;
    uint32_t qprio = 0; // TO-DO: handle priority and stuff
    sqc.cdw11 = (pc & 1u) | ((qprio & 3u) << 1) | ((cqid & 0xFFFFu) << 16);

    cid = nvme_submit_command(nvme, 0x01, sqc, nvme->admin_queue);
    if (cid != UINT16_MAX) nvme_wait_completion(nvme, cid, nvme->admin_queue);

    // doorbells init
    NVME_SQ_TDBL(nvme->registers, qid, nvme->db_stride) = 0;
    NVME_CQ_HDBL(nvme->registers, qid, nvme->db_stride) = 0;

    log(LOG_INFO, "Created IO QP qid=%u depth=%u SQ(p)=0x%llx CQ(p)=0x%llx",
        qid, NVME_IO_QUEUE_DEPTH,
        (unsigned long long)ioq->sq_dma.paddr,
        (unsigned long long)ioq->cq_dma.paddr);
}

// --- ACTUAL COMMANDS ---

static void nvme_identify_controller(nvme_t *nvme)
{
    dma_buf_t id_dma = dma_alloc(4096);
    if (!id_dma.vaddr)
    {
        log(LOG_ERROR, "identify controller: dma alloc failed");
        return;
    }
    memset(id_dma.vaddr, 0, 4096);

    nvme_command_t cmd = (nvme_command_t){0};
    cmd.dptr.prp1 = id_dma.paddr;
    cmd.cdw10 = 1;

    uint16_t cid = nvme_submit_command(nvme, 0x06, cmd, nvme->admin_queue);
    if (cid != UINT16_MAX)
        nvme_wait_completion(nvme, cid, nvme->admin_queue);

    nvme->identity = (nvme_cid_t *)vm_alloc(4096);
    memcpy(nvme->identity, id_dma.vaddr, 4096);

    uint8_t *id = (uint8_t *)nvme->identity;

    // TO-DO: Fix this bullshit
    uint16_t vid   = *(uint16_t *)(id + 0x000);
    uint16_t ssvid = *(uint16_t *)(id + 0x002);
    uint8_t  mdts  = *(uint8_t  *)(id + 0x04D);
    uint32_t nn    = *(uint32_t *)(id + 0x204);

    uint32_t vs    = nvme->registers->VS;

    log(LOG_INFO, "NVMe Identify: VID=%04x SSVID=%04x VS=0x%08x NN(max_nsid)=%u MDTS=%u",
        vid, ssvid, vs, nn, mdts);

    char sn[21], mn[41], fr[9];
    memcpy(sn, id + 0x004, 20); sn[20] = 0;
    memcpy(mn, id + 0x018, 40); mn[40] = 0;
    memcpy(fr, id + 0x040, 8);  fr[8]  = 0;

    // trim trailing spaces
    for (int i = 19; i >= 0 && sn[i] == ' '; i--) sn[i] = 0;
    for (int i = 39; i >= 0 && mn[i] == ' '; i--) mn[i] = 0;
    for (int i = 7;  i >= 0 && fr[i] == ' '; i--) fr[i] = 0;

    log(LOG_INFO, "NVMe Identify: SN='%s' MN='%s' FR='%s'", sn, mn, fr);

    dma_free(&id_dma);
}

int nvme_read(drive_t *d, const void *buf, uint64_t lba, uint64_t count)
{
    nvme_namespace_t *ns = (nvme_namespace_t*)d->device.driver_data;
    nvme_t *nvme = ns->controller;
    nvme_queue_t *ioq = nvme->io_queue;

    uint8_t *out = (uint8_t *)(uintptr_t)buf; // why
    dma_buf_t dma = dma_alloc(4096); // single 4k dma page for prp1-only for now

    uint64_t remaining = count;
    uint64_t cur_lba = lba;

    while (remaining)
    {
        //how many LBAs fit in one PRP1 page?
        uint64_t max_blocks = 4096ull / (uint64_t)ns->lba_size;
        if (max_blocks == 0)
        {
            dma_free(&dma);
            return -1;
        }

        uint64_t blocks = (remaining < max_blocks) ? remaining : max_blocks;
        uint64_t bytes  = blocks * (uint64_t)ns->lba_size;

        memset(dma.vaddr, 0, 4096);

        // create NVME read command
        nvme_command_t cmd = (nvme_command_t){0};
        cmd.nsid = ns->nsid;
        cmd.dptr.prp1 = dma.paddr;
        cmd.dptr.prp2 = 0;

        // explain
        cmd.cdw10 = (uint32_t)(cur_lba & 0xFFFFFFFFu);
        cmd.cdw11 = (uint32_t)(cur_lba >> 32);
        cmd.cdw12 = (uint32_t)(((blocks - 1) & 0xFFFFu)); // NLB = blocks-1

        uint16_t cid = nvme_submit_command(nvme, 0x02, cmd, ioq);
        if (cid == UINT16_MAX)
        {
            dma_free(&dma);
            return -1;
        }
        nvme_wait_completion(nvme, cid, ioq);

        memcpy(out, dma.vaddr, (size_t)bytes);

        out += bytes;
        cur_lba += blocks;
        remaining -= blocks;

    }

    dma_free(&dma);
    return 0;
}

int nvme_write(drive_t *d, const void *buf, uint64_t lba, uint64_t count)
{
    nvme_namespace_t *ns = (nvme_namespace_t*)d->device.driver_data;
    nvme_t *nvme = ns->controller;
    nvme_queue_t *ioq = nvme->io_queue;

    const uint8_t *in = (uint8_t *)(uintptr_t)buf; // why
    dma_buf_t dma = dma_alloc(4096); // single 4k dma page for prp1-only for now

    uint64_t remaining = count;
    uint64_t cur_lba = lba;

    while (remaining)
    {
        uint64_t max_blocks = 4096ull / (uint64_t)ns->lba_size;
        if (max_blocks == 0)
        {
            dma_free(&dma);
            return -1;
        }

        uint64_t blocks = (remaining < max_blocks) ? remaining : max_blocks;
        uint64_t bytes  = blocks * (uint64_t)ns->lba_size;

        memset(dma.vaddr, 0, 4096);
        memcpy(dma.vaddr, in, (size_t)bytes);

        // create NVME write command
        nvme_command_t cmd = (nvme_command_t){0};
        cmd.nsid = ns->nsid;
        cmd.dptr.prp1 = dma.paddr;
        cmd.dptr.prp2 = 0;
        cmd.cdw10     = (uint32_t)(cur_lba & 0xFFFFFFFFu);
        cmd.cdw11     = (uint32_t)(cur_lba >> 32);
        cmd.cdw12     = (uint32_t)(((blocks - 1) & 0xFFFFu)); // NLB

        uint16_t cid = nvme_submit_command(nvme, 0x01, cmd, ioq);
        if (cid == UINT16_MAX)
        {
            dma_free(&dma);
            return -1;
        }
        nvme_wait_completion(nvme, cid, ioq);

        in += bytes;
        cur_lba += blocks;
        remaining -= blocks;
    }

    dma_free(&dma);
    return 0;
}

static void nvme_identify_namespace(nvme_t *nvme)
{
    ASSERT(nvme);
    ASSERT(nvme->identity);

    // get active NSID list
    dma_buf_t list_dma = dma_alloc(4096);
    memset(list_dma.vaddr, 0, 4096);

    nvme_command_t list_cmd = (nvme_command_t){0};
    list_cmd.dptr.prp1 = list_dma.paddr;
    list_cmd.cdw10 = 0x02; // CNS=2 - active namespace ID list
    list_cmd.nsid = 0;

    uint16_t list_cid = nvme_submit_command(nvme, 0x06, list_cmd, nvme->admin_queue);
    if (list_cid != UINT16_MAX)
        nvme_wait_completion(nvme, list_cid, nvme->admin_queue);

    uint32_t *nsids = (uint32_t *)list_dma.vaddr;

    for (uint32_t i = 0; i < 1024; i++)
    {
        uint32_t nsid = nsids[i];
        if (nsid == 0) break;

        dma_buf_t ns_dma = dma_alloc(sizeof(nvme_nsidn_t));
        memset(ns_dma.vaddr, 0, sizeof(nvme_nsidn_t));

        nvme_command_t identify_ns = (nvme_command_t){
            .nsid = nsid,
            .dptr.prp1 = ns_dma.paddr,
            .cdw10 = 0x00 // CNS=0 - identify namespace
        };

        uint16_t cid = nvme_submit_command(nvme, 0x06, identify_ns, nvme->admin_queue);
        if (cid != UINT16_MAX)
            nvme_wait_completion(nvme, cid, nvme->admin_queue);

        // LOGGING
        nvme_nsidn_t *ns = (nvme_nsidn_t *)ns_dma.vaddr;
               log(LOG_INFO, "Namespace Identify: NSID=%u NSZE=%llu NCAP=%llu FLBAS=0x%02x",
                   nsid,
                   (unsigned long long)ns->nsze,
                   (unsigned long long)ns->ncap,
                   (unsigned)ns->flbas);

        // add namespace as storage drive
        // create namespace object
        uint8_t *buf = (uint8_t *)ns_dma.vaddr;

        uint64_t ncap  = *(uint64_t *)(buf + 0x08);
        uint8_t  flbas = *(uint8_t  *)(buf + 0x1A);
        uint8_t  idx   = flbas & 0x0F;
        uint8_t  lbads = *(uint8_t  *)(buf + 0x80 + idx * 16 + 2);
        uint32_t lba_size = 1u << lbads;

        nvme_namespace_t *nsobj = heap_alloc(sizeof(nvme_namespace_t)); // TO-DO: free this at some point
        memset(nsobj, 0, sizeof(*nsobj));
        nsobj->controller = nvme;
        nsobj->nsid = nsid;
        nsobj->lba_count = ncap;
        nsobj->lba_size = lba_size;

        // create drive object
        drive_t *d = drive_create(DRIVE_TYPE_NVME);

        d->sector_size = lba_size;
        d->sectors = ncap;

        d->serial = nvme->identity->sn;
        d->model = nvme->identity->mn;
        d->vendor = "NVMe";
        d->revision = NULL;

        d->read_sectors = nvme_read;
        d->write_sectors = nvme_write;

        // assing namespace to drive
        d->device.driver_data = nsobj;

        drive_mount(d);

        log(LOG_INFO, "Mount nsid=%u ncap=%llu lbads=%u lba_size=%u",
            nsid, (unsigned long long)ncap, lbads, lba_size);

        uint8_t *tmp = vm_alloc(4096);
        memset(tmp, 0, 4096);

        // --- WRITE test pattern to LBA 8 ---
        const uint64_t test_lba = 8;
        const uint64_t test_cnt = 1; // 1 sector (512B on your namespace)

        const char *msg = "LYKOS NVME WRITE/READ TEST\n";
        size_t msg_len = strlen(msg);
        if (msg_len > d->sector_size) msg_len = d->sector_size;

        memcpy(tmp, msg, msg_len);

        // fill the rest of the sector with a simple pattern
        for (uint64_t i = msg_len; i < d->sector_size; i++)
            tmp[i] = (uint8_t)(i & 0xFF);

        int wrc = d->write_sectors(d, tmp, test_lba, test_cnt);
        log(LOG_INFO, "nvme_write test wrc=%d lba=%llu cnt=%llu",
            wrc, (unsigned long long)test_lba, (unsigned long long)test_cnt);

        // --- READ back from LBA 8 ---
        memset(tmp, 0xAA, 4096);

        int rrc = d->read_sectors(d, tmp, test_lba, test_cnt);
        log(LOG_INFO, "nvme_readback rrc=%d bytes[0..15]=%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
            rrc,
            tmp[0], tmp[1], tmp[2], tmp[3], tmp[4], tmp[5], tmp[6], tmp[7],
            tmp[8], tmp[9], tmp[10], tmp[11], tmp[12], tmp[13], tmp[14], tmp[15]);

        // optional: print ASCII prefix (up to 32 chars)
        char asc[33];
        for (int i = 0; i < 32; i++)
        {
            uint8_t c = tmp[i];
            asc[i] = (c >= 32 && c <= 126) ? (char)c : '.';
        }
        asc[32] = 0;
        log(LOG_INFO, "nvme_readback ascii32='%s'", asc);

        vm_free(tmp);

        dma_free(&ns_dma);
    }
    dma_free(&list_dma);
}

// --- INIT ---

void nvme_init(volatile pci_header_type0_t *header)
{
    log(LOG_DEBUG, "Starting NVMe init...");

    volatile uint16_t *cmd = (volatile uint16_t *)((volatile uint8_t *)header + 0x04);
    uint16_t v = *cmd;

    v |= (1u << 1) | (1u << 2);        // MEM + BUS MASTER
    v &= (uint16_t)~(1u << 10);        // clear INTx disable (allow legacy INTx for now)

    *cmd = v;
    (void)*cmd;

    // map BAR0 - for now assumes it's 64bit
    uint64_t bar0_phys =
        ((uint64_t)header->bar[1] << 32) | ((uint64_t)header->bar[0] & 0xFFFFFFF0u);

    nvme_t *nvme = vm_alloc(sizeof(nvme_t));
    memset(nvme, 0, sizeof(*nvme));

    nvme->registers = (nvme_regs_t *)mmio_map((uintptr_t)bar0_phys, 0x4000);
    if (!nvme->registers)
    {
        log(LOG_ERROR, "mmio_map BAR0 failed");
        return;
    }

    // doorbell stride = 4 << CAP.DSTRD
    nvme_cap_t *cap = (nvme_cap_t *)&nvme->registers->CAP;
    nvme->db_stride = 4u << cap->dstrd;

    nvme_reset(nvme);
    nvme_create_admin_queue(nvme);
    nvme_start(nvme);
    nvme_identify_controller(nvme);
    nvme_create_io_queue(nvme);
    nvme_identify_namespace(nvme);
}
