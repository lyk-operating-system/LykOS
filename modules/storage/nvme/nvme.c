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

static nvme_cq_entry_t *nvme_poll_cq(nvme_t *nvme, nvme_queue_t *queue)
{
    (void)nvme;

    nvme_cq_entry_t *entry = &queue->cq[queue->head];

    if ((entry->phase & 1) != queue->phase)
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
    uint64_t timeout = 1000000;
    while (nvme->registers->CSTS.rdy != ready && timeout--)
        arch_lcpu_relax();
    if (timeout == 0)
        log(LOG_ERROR, "NVMe wait_ready timeout");
}

// --- BASIC FUNCS ---

void nvme_reset(nvme_t *nvme)
{
    log(LOG_DEBUG, "Entered reset func");

    // Spec wants: set EN=0 then wait RDY=0
    nvme->registers->CC.en = 0;
    nvme_wait_ready(nvme, false);
}

void nvme_start(nvme_t *nvme)
{
    nvme->registers->CC.ams = 0;
    nvme->registers->CC.mps = 0;
    nvme->registers->CC.css = 0;

    nvme->registers->CC.iosqes = 6;
    nvme->registers->CC.iocqes = 4;

    nvme->registers->CC.en = 1;
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
    aq->cq = (nvme_cq_entry_t *)aq->cq_dma.vaddr;

    memset(aq->sq, 0, sq_size);
    memset(aq->cq, 0, cq_size);

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

    __atomic_thread_fence(__ATOMIC_SEQ_CST);

    aq->tail = next_tail;
    NVME_SQ_TDBL(nvme->registers, aq->qid, nvme->db_stride) = aq->tail;

    spinlock_release(&aq->lock);
    return cid;
}

static void nvme_admin_wait_completion(nvme_t *nvme, uint16_t cid)
{
    uint64_t timeout = 1000000;
    while (timeout--)
    {
        nvme_cq_entry_t *entry = nvme_poll_cq(nvme, nvme->admin_queue);
        if (entry && entry->cid == cid)
            return;
    }
    log(LOG_ERROR, "NVMe admin command CID=%u timed out", cid);
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

    nvme->identity = (nvme_cid_t *)vm_alloc(sizeof(nvme_cid_t));
    memcpy(nvme->identity, id_dma.vaddr, sizeof(nvme_cid_t));

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

void nvme_init(pci_header_type0_t *header)
{
    log(LOG_DEBUG, "Entered nvme init function.");

    // TO-DO: Add proper PCI config!!!!!!!!!!!!!! this does nothing
    header->common.command |= (1 << 1) | (1 << 2);

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
    nvme_identify_namespace(nvme);
}
