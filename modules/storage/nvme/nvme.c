#include "mm/dma.h"
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

static inline uint32_t mmio_read32(volatile void *base, uintptr_t off)
{
    return *(volatile uint32_t *)((uintptr_t)base + off);
}
static inline void mmio_write32(volatile void *base, uintptr_t off, uint32_t v)
{
    *(volatile uint32_t *)((uintptr_t)base + off) = v;
}

#define NVME_CC_OFF   0x14
#define NVME_CSTS_OFF 0x1C

static void nvme_wait_ready(nvme_t *nvme, bool ready)
{
    nvme_cap_t cap = nvme->registers->CAP;
    uint32_t to = cap.to ? cap.to : 1;
    uint64_t timeout = (uint64_t)to * 5000000ull;

    while (nvme->registers->CSTS.rdy != ready && timeout--)
        arch_lcpu_relax();

    if (timeout == 0)
    {
        uint32_t csts = mmio_read32(nvme->registers, NVME_CSTS_OFF);
        uint32_t cc   = mmio_read32(nvme->registers, NVME_CC_OFF);
        log(LOG_ERROR, "wait_ready timeout want=%d CSTS=0x%08x CC=0x%08x", ready, csts, cc);
    }
}

// --- BASIC FUNCS ---

void nvme_reset(nvme_t *nvme)
{
    log(LOG_DEBUG, "Entered reset func");

    // Spec wants: set EN=0 then wait RDY=0
    nvme->registers->CC.en = 0;
    nvme_wait_ready(nvme, false);
}


#define CC_EN          (1u << 0)
#define CC_CSS_SHIFT   4
#define CC_MPS_SHIFT   7
#define CC_AMS_SHIFT   11
#define CC_IOSQES_SHIFT 16
#define CC_IOCQES_SHIFT 20


void nvme_start(nvme_t *nvme)
{
    uint32_t cc = 0;
    cc |= (0u << CC_CSS_SHIFT);      // NVM
    cc |= (0u << CC_MPS_SHIFT);      // 4KiB
    cc |= (0u << CC_AMS_SHIFT);      // round-robin
    cc |= (6u << CC_IOSQES_SHIFT);   // 64B
    cc |= (4u << CC_IOCQES_SHIFT);   // 16B
    cc |= CC_EN;

    mmio_write32(nvme->registers, NVME_CC_OFF, cc);
    (void)mmio_read32(nvme->registers, NVME_CC_OFF);

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

static uint16_t nvme_submit_admin_command(nvme_t *nvme, uint8_t opc, nvme_command_t command)
{
    nvme_queue_t *aq = nvme->admin_queue;

    spinlock_acquire(&aq->lock);

    uint16_t cid = UINT16_MAX;
    for (uint16_t i = 0; i < aq->depth; i++)
    {
        uint16_t try = (aq->next_cid + i) % aq->depth;
        if (!aq->cid_used[try])
        {
            cid = try;
            aq->cid_used[try] = true;
            aq->next_cid = (try + 1) % aq->depth;
            break;
        }
    }

    if (cid == UINT16_MAX)
    {
        spinlock_release(&aq->lock);
        return UINT16_MAX;
    }

    uint16_t old_tail = aq->tail;
    uint16_t next_tail = (aq->tail + 1) % aq->depth;
    if (next_tail == aq->head)
    {
        aq->cid_used[cid] = false;
        spinlock_release(&aq->lock);
        return UINT16_MAX;
    }

    nvme_sq_entry_t e = {0};
    e.opc = opc;
    e.cid = cid;
    e.psdt = 0;
    e.command = command;

    aq->sq[old_tail] = e;

    volatile uint8_t *v_opc = (volatile uint8_t *)&aq->sq[old_tail].opc;
    volatile uint8_t *p_opc = (volatile uint8_t *)(HHDM + aq->sq_dma.paddr + (old_tail * sizeof(nvme_sq_entry_t)));

    log(LOG_DEBUG, "SQE opc via vaddr=%02x via HHDM(paddr)=%02x vaddr=%p paddr_alias=%p",
        *v_opc, *p_opc, v_opc, p_opc);


    __atomic_thread_fence(__ATOMIC_SEQ_CST);

    aq->tail = next_tail;
    NVME_SQ_TDBL(nvme->registers, aq->qid, nvme->db_stride) = aq->tail;

    spinlock_release(&aq->lock);
    return cid;
}

// static void nvme_admin_wait_completion(nvme_t *nvme, uint16_t cid)
// {
//     nvme_cap_t cap = nvme->registers->CAP;
//     uint64_t timeout = (uint64_t)(cap.to ? cap.to : 1) * 5000000ull;

//     while (timeout--)
//     {
//         volatile nvme_cq_entry_t *entry = nvme_poll_cq(nvme, nvme->admin_queue);
//         if (entry && entry->cid == cid)
//         {
//             if (entry->status != 0)
//                 log(LOG_ERROR, "Admin cid=%u failed status=0x%x", cid, entry->status);
//             return;
//         }
//     }

//     nvme_queue_t *aq = nvme->admin_queue;
//     volatile nvme_cq_entry_t *e0 = &aq->cq[aq->head];
//     //uint32_t sq_db = NVME_SQ_TDBL(nvme->registers, 0, nvme->db_stride);
//     //uint32_t cq_db = NVME_CQ_HDBL(nvme->registers, 0, nvme->db_stride);

//     log(LOG_ERROR,
//         "Admin cid=%u timed out. AQ head=%u tail=%u phase=%u CQ[head]: cid=%u st=0x%x ph=%u sqh=%u sqid=%u status_p=0x%04x",
//         cid, aq->head, aq->tail, aq->phase,
//         e0->cid, NVME_CQE_STATUS(e0), NVME_CQE_PHASE(e0), e0->sq_head, e0->sq_id, e0->status);
// }

static void nvme_admin_wait_completion(nvme_t *nvme, uint16_t cid)
{
    nvme_cap_t cap = nvme->registers->CAP;
    uint64_t timeout = (uint64_t)(cap.to ? cap.to : 1) * 5000000ull;

    while (timeout--)
    {
        volatile nvme_cq_entry_t *entry = nvme_poll_cq(nvme, nvme->admin_queue);
        if (entry && entry->cid == cid)
        {
            // Read raw DW3 of CQE (bytes 12..15). This avoids any bitfield/layout issues.
            uint32_t dw3 = *(volatile uint32_t *)((volatile uint8_t *)entry + 12);

            uint8_t  p   = (uint8_t)((dw3 >> 16) & 0x1u);
            uint8_t  sc  = (uint8_t)((dw3 >> 17) & 0xFFu);
            uint8_t  sct = (uint8_t)((dw3 >> 25) & 0x7u);
            uint8_t  dnr = (uint8_t)((dw3 >> 31) & 0x1u);

            if (sct != 0 || sc != 0)
                log(LOG_ERROR, "Admin cid=%u failed: SCT=%u SC=0x%02x DNR=%u DW3=0x%08x P=%u",
                    cid, sct, sc, dnr, dw3, p);

            return;
        }
    }

    nvme_queue_t *aq = nvme->admin_queue;
    volatile nvme_cq_entry_t *e0 = &aq->cq[aq->head];
    uint32_t dw3 = *(volatile uint32_t *)((volatile uint8_t *)e0 + 12);

    uint8_t  p   = (uint8_t)((dw3 >> 16) & 0x1u);
    uint8_t  sc  = (uint8_t)((dw3 >> 17) & 0xFFu);
    uint8_t  sct = (uint8_t)((dw3 >> 25) & 0x7u);
    uint8_t  dnr = (uint8_t)((dw3 >> 31) & 0x1u);

    log(LOG_ERROR,
        "Admin cid=%u timed out. AQ head=%u tail=%u phase=%u CQ[head]: cid=%u DW3=0x%08x P=%u SCT=%u SC=0x%02x DNR=%u",
        cid, aq->head, aq->tail, aq->phase, e0->cid, dw3, p, sct, sc, dnr);
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

    uint16_t cid = nvme_submit_admin_command(nvme, 0x06, cmd);
    if (cid != UINT16_MAX)
        nvme_admin_wait_completion(nvme, cid);

    nvme->identity = (nvme_cid_t *)vm_alloc(4096);
    memcpy(nvme->identity, id_dma.vaddr, 4096);

    uint8_t *id = (uint8_t *)nvme->identity;

    uint16_t vid   = *(uint16_t *)(id + 0x000);
    uint16_t ssvid = *(uint16_t *)(id + 0x002);
    uint8_t  mdts  = *(uint8_t  *)(id + 0x04D);
    uint32_t nn    = *(uint32_t *)(id + 0x204);

    // Version comes from MMIO register VS, not from id+0x008
    uint32_t vs    = nvme->registers->VS;

    log(LOG_INFO, "NVMe Identify: VID=%04x SSVID=%04x VS=0x%08x NN(max_nsid)=%u MDTS=%u",
        vid, ssvid, vs, nn, mdts);

    dma_free(&id_dma);
}

int nvme_read(drive_t *d, const void *buf, uint64_t lba, uint64_t count)
{
    (void)d; (void)buf; (void)lba; (void)count;
    return 0;
}

int nvme_write(drive_t *d, const void *buf, uint64_t lba, uint64_t count)
{
    (void)d; (void)buf; (void)lba; (void)count;
    return 0;
}

static void nvme_identify_namespace(nvme_t *nvme)
{
    ASSERT(nvme);
    ASSERT(nvme->identity);

    uint32_t nn = nvme->identity->nn;
    for (uint32_t nsid = 1; nsid <= nn; nsid++)
    {
        dma_buf_t ns_dma = dma_alloc(sizeof(nvme_nsidn_t));
        memset(ns_dma.vaddr, 0, sizeof(nvme_nsidn_t));

        nvme_command_t identify_ns = (nvme_command_t){
            .nsid = nsid,
            .dptr.prp1 = ns_dma.paddr,
            .cdw10 = 0x00
        };

        uint16_t cid = nvme_submit_admin_command(nvme, 0x06, identify_ns);
        if (cid != UINT16_MAX)
            nvme_admin_wait_completion(nvme, cid);

        dma_free(&ns_dma);
    }
}

// --- INIT ---

void nvme_init(volatile pci_header_type0_t *header)
{
    log(LOG_DEBUG, "Entered nvme init function.");

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
    log(LOG_DEBUG, "did escape reset");
    nvme_create_admin_queue(nvme);
    log(LOG_DEBUG, "did create admin queue");
    nvme_start(nvme);
    log(LOG_DEBUG, "did escape start");
    log(LOG_INFO, "AQ: ASQ(p)=0x%llx ACQ(p)=0x%llx AQA.asqs=%u AQA.acqs=%u",
        (unsigned long long)nvme->admin_queue->sq_dma.paddr,
        (unsigned long long)nvme->admin_queue->cq_dma.paddr,
        nvme->registers->AQA.asqs, nvme->registers->AQA.acqs);

    log(LOG_INFO, "CSTS=0x%08x CC=0x%08x",
        *(volatile uint32_t *)&nvme->registers->CSTS,
        *(volatile uint32_t *)&nvme->registers->CC);

    nvme_identify_controller(nvme);
    nvme_identify_namespace(nvme);
}
