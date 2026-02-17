#include "nvme.h"
#include "nvme_hw.h"

#include "arch/lcpu.h"
#include "dev/bus/pci.h"
#include "dev/storage/drive.h"
#include "log.h"
#include "mm/mmio.h"
#include "mm/heap.h"
#include "mm/vm.h"
#include "sync/spinlock.h"
#include <stdint.h>

// --- LOGGING HELPERS ---

static void nvme_copy_trim(char *dst, size_t dstsz, const char *src, size_t srclen)
{
    size_t n = (srclen < dstsz - 1) ? srclen : (dstsz - 1);
    memcpy(dst, src, n);
    dst[n] = 0;

    // trim trailing spaces
    for (size_t i = (size_t)n - 1; i >= 0 && dst[i] == ' '; --i)
        dst[i] = 0;
}


// --- FUNCTIONAL HELPERS ---

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

static void nvme_wait_ready(nvme_t *nvme, bool ready)
{
    uint64_t cap_raw = mmio_read64(&nvme->registers->CAP);
    uint32_t to = NVME_CAP_TO(cap_raw);
    if (to == 0) to = 1;
    uint64_t timeout = (uint64_t)to * 5000000ull;


    while ((((mmio_read32(&nvme->registers->CSTS) & NVME_CSTS_RDY) != 0) ? 1 : 0) != (ready ? 1 : 0) && timeout--)
        arch_lcpu_relax();

    if (timeout == 0)
    {
        uint32_t csts = mmio_read32(&nvme->registers->CSTS);
        uint32_t cc   = mmio_read32(&nvme->registers->CC);
        log(LOG_ERROR, "wait_ready timeout want=%d CSTS=0x%08x CC=0x%08x", ready, csts, cc);
    }
}

// --- BASIC FUNCS ---

void nvme_reset(nvme_t *nvme)
{
    uint32_t cc = mmio_read32(&nvme->registers->CC);
    cc &= ~NVME_CC_EN;
    mmio_write32(&nvme->registers->CC, cc);

    nvme_wait_ready(nvme, false);
}

void nvme_start(nvme_t *nvme)
{
    uint32_t cc = 0;
    cc |= (0u << NVME_CC_CSS_SHIFT);     // NVM
    cc |= (0u << NVME_CC_MPS_SHIFT);     // 4KiB
    cc |= (0u << NVME_CC_AMS_SHIFT);     // RR
    cc |= (6u << NVME_CC_IOSQES_SHIFT);  // 64B
    cc |= (4u << NVME_CC_IOCQES_SHIFT);  // 16B
    cc |= NVME_CC_EN;

    mmio_write32(&nvme->registers->CC, cc);
    (void)mmio_read32(&nvme->registers->CC); // flush
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

    aq->qid = nvme->next_qid++;
    aq->depth = NVME_ADMIN_QUEUE_DEPTH;
    aq->head = 0;
    aq->tail = 0;
    aq->phase = 1;

    aq->next_cid = 0;
    memset(aq->cid_used, 0, sizeof(aq->cid_used));
    aq->lock = SPINLOCK_INIT;

    uint32_t aqa = 0;
    aqa |= ((NVME_ADMIN_QUEUE_DEPTH - 1) & 0x0FFFu);         // ASQS bits 11:0
    aqa |= (((NVME_ADMIN_QUEUE_DEPTH - 1) & 0x0FFFu) << 16); // ACQS bits 27:16
    mmio_write32(&nvme->registers->AQA, aqa);

    mmio_write64(&nvme->registers->ASQ, aq->sq_dma.paddr);
    mmio_write64(&nvme->registers->ACQ, aq->cq_dma.paddr);
}

// --- COMMAND FUNCTIONS ---

static uint16_t nvme_submit_command(nvme_t *nvme, uint8_t opc, nvme_command_t command, nvme_queue_t *q)
{
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
    uint64_t cap_raw = mmio_read64(&nvme->registers->CAP);
    uint32_t to = NVME_CAP_TO(cap_raw);
    if (to == 0) to = 1;
    uint64_t timeout = (uint64_t)to * 5000000ull;

    while (timeout--)
    {
        volatile nvme_cq_entry_t *entry = nvme_poll_cq(nvme, q);
        if (entry && entry->cid == cid)
        {
            // decode raw DW3 for error details
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
    const uint16_t qid = nvme->next_qid++;

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
}

// --- ACTUAL COMMANDS ---

int nvme_read(drive_t *d, void *buf, uint64_t lba, uint64_t count)
{
    nvme_namespace_t *ns = (nvme_namespace_t*)d->device.driver_data;
    nvme_t *nvme = ns->controller;
    nvme_queue_t *ioq = nvme->io_queue;

    uint8_t *out = (uint8_t *)buf; // make it pointer to bytes
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

        cmd.cdw10 = (uint32_t)(cur_lba & 0xFFFFFFFFu); // low half
        cmd.cdw11 = (uint32_t)(cur_lba >> 32);         // high half
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

    const uint8_t *in = (const uint8_t *)buf; // make it pointer to bytes
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

// --- IDENTIFY COMMANDS ---
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

    char sn[21], mn[41], fr[9];
    nvme_copy_trim(sn, sizeof(sn), nvme->identity->sn, 20);
    nvme_copy_trim(mn, sizeof(mn), nvme->identity->mn, 40);
    nvme_copy_trim(fr, sizeof(fr), nvme->identity->fr, 8);

    log(LOG_INFO, "NVMe Identify: SN='%s' MN='%s' FR='%s'", sn, mn, fr);

    dma_free(&id_dma);
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

        /* LOGGING
        *//*
               log(LOG_INFO, "Namespace Identify: NSID=%u NSZE=%llu NCAP=%llu FLBAS=0x%02x",
                   nsid,
                   (unsigned long long)ns->nsze,
                   (unsigned long long)ns->ncap,
                   (unsigned)ns->flbas);
                   */

        // here we add namespace as a storage drive

        // create namespace object
        nvme_nsidn_t *ns = (nvme_nsidn_t *)ns_dma.vaddr;
        uint64_t ncap  = ns->ncap;
        uint8_t  flbas = ns->flbas;
        uint8_t  idx   = flbas & 0x0F;
        const uint8_t *lbaf = (const uint8_t *)ns + 0x80;
        uint8_t lbads = lbaf[idx * 16 + 2];

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

        log(LOG_INFO, "Mounted device: nsid=%u ncap=%llu lbads=%u lba_size=%u",
            nsid, (unsigned long long)ncap, lbads, lba_size);

        dma_free(&ns_dma);
    }
    dma_free(&list_dma);
}

// --- INIT ---

void nvme_init(volatile pci_header_type0_t *header)
{
    volatile uint16_t *cmd = (volatile uint16_t *)((volatile uint8_t *)header + 0x04);
    uint16_t v = *cmd;

    v |= (1u << 1) | (1u << 2);        // MEM + BUS MASTER
    v &= (uint16_t)~(1u << 10);        // clear INTx disable

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
    uint64_t cap_raw = mmio_read64(&nvme->registers->CAP);
    nvme->db_stride = 4u << NVME_CAP_DSTRD(cap_raw);

    nvme_reset(nvme);
    nvme_create_admin_queue(nvme);
    nvme_start(nvme);
    nvme_identify_controller(nvme);
    nvme_create_io_queue(nvme);
    nvme_identify_namespace(nvme);
}
