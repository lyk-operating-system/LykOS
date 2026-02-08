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

// poll the completion queue once; returns pointer to a valid completed entry or NULL
static nvme_cq_entry_t *nvme_poll_cq(nvme_t *nvme, nvme_queue_t *queue)
{
    nvme_cq_entry_t *entry = &queue->cq[queue->head];

    if ((entry->phase & 1) != queue->phase)
        return NULL; // nothing new

    uint16_t cid = entry->cid;

    // got a new entry
    queue->head = (queue->head + 1) % queue->depth;
    if (queue->head == 0)
        queue->phase ^= 1;

    // tell controller entry is consumed
    NVME_CQ_HDBL(nvme->registers, queue->qid, nvme->db_stride) = queue->head;

    // free CID
    spinlock_acquire(&queue->lock);
    queue->cid_used[cid] = false;
    spinlock_release(&queue->lock);

    return entry;
}

// set ready status and wait
static void nvme_wait_ready(nvme_t *nvme, bool ready)
{
    uint64_t timeout = 1000000;
    while (nvme->registers->CSTS.rdy != ready && timeout--)
        arch_lcpu_relax(); // or a small delay
    if (timeout == 0)
        log(LOG_ERROR, "NVMe wait_ready timeout");
}

// --- BASIC FUNCS ---
// void nvme_reset(nvme_t *nvme)
// {
//     log(LOG_DEBUG, "Entered reset func");

//     uint64_t timeout = 1000000; // adjust as needed
//     if (nvme->registers->CC.en)
//     {
//         while (nvme->registers->CSTS.rdy && timeout--)
//             arch_lcpu_relax(); // or asm("pause") if you have one
//         if (timeout == 0)
//             log(LOG_WARN, "NVMe reset: CSTS.rdy never cleared");
//     }

//     nvme->registers->CC.en = 0;
// }

// testing func
// void nvme_reset(nvme_t *nvme)
// {
//     log(LOG_DEBUG, "Skipping .rdy wait (test mode)");
//     nvme->registers->CC.en = 0;
// }


// void nvme_start(nvme_t *nvme)
// {
//     nvme->registers->CC.ams = 0;
//     nvme->registers->CC.mps = 0; // 4kb page shift
//     nvme->registers->CC.css = 0;

//     // set queue entry sizes
//     nvme->registers->CC.iosqes = 6;
//     nvme->registers->CC.iocqes = 4;

//     nvme->registers->CC.en  = 1;
// }

// testing func
// void nvme_start(nvme_t *nvme)
// {
//     nvme->registers->CC.ams = 0;
//     nvme->registers->CC.mps = 0;
//     nvme->registers->CC.css = 0;

//     nvme->registers->CC.iosqes = 6;
//     nvme->registers->CC.iocqes = 4;

//     nvme->registers->CC.en  = 1; // just set it, don't wait
// }

// Remove the "testing" version and use proper reset
void nvme_reset(nvme_t *nvme)
{
    log(LOG_DEBUG, "Resetting NVMe controller");

    enum { NVME_OFF_CC = 0x14, NVME_OFF_CSTS = 0x1C };

    volatile uint32_t *cc   = (volatile uint32_t *)((uintptr_t)nvme->registers + NVME_OFF_CC);
    volatile uint32_t *csts = (volatile uint32_t *)((uintptr_t)nvme->registers + NVME_OFF_CSTS);

    uint32_t cc_before   = *cc;
    uint32_t csts_before = *csts;

    log(LOG_DEBUG, "MMIO base=%p CC(before)=%#x CSTS(before)=%#x (RDY=%u CFS=%u)",
        nvme->registers,
        cc_before,
        csts_before,
        (unsigned)(csts_before & 1u),
        (unsigned)((csts_before >> 1) & 1u));

    // Disable the controller: clear EN (bit 0) with a single 32-bit store
    uint32_t cc_after = cc_before & ~1u;

    log(LOG_DEBUG, "Writing CC=%#x (clearing EN)", cc_after);
    *cc = cc_after;

    // Read back to confirm write posted
    uint32_t cc_readback = *cc;
    uint32_t csts_mid    = *csts;

    log(LOG_DEBUG, "CC(after)=%#x (EN=%u) CSTS(mid)=%#x (RDY=%u CFS=%u)",
        cc_readback,
        (unsigned)(cc_readback & 1u),
        csts_mid,
        (unsigned)(csts_mid & 1u),
        (unsigned)((csts_mid >> 1) & 1u));

    // Wait for controller to acknowledge disable (CSTS.RDY = 0)
    uint64_t timeout = 1000000;
    while (((*csts) & 1u) && timeout--)
        arch_lcpu_relax();

    uint32_t csts_after = *csts;
    log(LOG_DEBUG, "CSTS(after)=%#x (RDY=%u CFS=%u) timeout_left=%lu",
        csts_after,
        (unsigned)(csts_after & 1u),
        (unsigned)((csts_after >> 1) & 1u),
        (unsigned long)timeout);

    if (timeout == 0)
        log(LOG_WARN, "NVMe reset: timeout waiting for CSTS.rdy=0");
}


// Remove the "testing" version and use proper start
void nvme_start(nvme_t *nvme)
{
    log(LOG_DEBUG, "Starting NVMe controller");

    nvme->registers->CC.ams = 0;      // Arbitration mechanism
    nvme->registers->CC.mps = 0;      // Memory page size (4KB)
    nvme->registers->CC.css = 0;      // I/O command set (NVM)
    nvme->registers->CC.iosqes = 6;   // I/O SQ entry size (2^6 = 64 bytes)
    nvme->registers->CC.iocqes = 4;   // I/O CQ entry size (2^4 = 16 bytes)

    // Enable the controller
    nvme->registers->CC.en = 1;

    // Wait for controller to be ready (CSTS.RDY = 1)
    uint64_t timeout = 1000000;
    while (!nvme->registers->CSTS.rdy && timeout--)
        arch_lcpu_relax();

    if (timeout == 0)
        log(LOG_ERROR, "NVMe start: timeout waiting for CSTS.rdy=1");
    else
        log(LOG_DEBUG, "NVMe controller is ready");
}

// --- ADMIN FUNCS ---
// TO-DO: add error handling
static void nvme_create_admin_queue(nvme_t *nvme)
{
    size_t sq_size = NVME_ADMIN_QUEUE_DEPTH * sizeof(nvme_sq_entry_t);
    size_t cq_size = NVME_ADMIN_QUEUE_DEPTH * sizeof(nvme_cq_entry_t);

    nvme_queue_t *aq = vm_alloc(sizeof(nvme_queue_t));
    nvme->admin_queue = aq;

    nvme->admin_queue->sq_dma = dma_alloc(sq_size);
    nvme->admin_queue->cq_dma = dma_alloc(cq_size);

    memset(nvme->admin_queue->sq, 0, sq_size);
    memset(nvme->admin_queue->cq, 0, cq_size);

    nvme->admin_queue->qid = 0; // Admin queue ID is 0
    nvme->admin_queue->depth = NVME_ADMIN_QUEUE_DEPTH;
    nvme->admin_queue->head = 0;
    nvme->admin_queue->tail = 0;
    nvme->admin_queue->phase = 1;

    // init lock and cids
    nvme->admin_queue->next_cid = 0;
    memset(nvme->admin_queue->cid_used, 0, sizeof(nvme->admin_queue->cid_used));
    nvme->admin_queue->lock = SPINLOCK_INIT;

    // set queue sizes in AQA register
    nvme->registers->AQA.asqs = NVME_ADMIN_QUEUE_DEPTH - 1;
    nvme->registers->AQA.acqs = NVME_ADMIN_QUEUE_DEPTH - 1;

    // program controller registers with physical addresses
    nvme->registers->ASQ = nvme->admin_queue->sq_dma.paddr;
    nvme->registers->ACQ = nvme->admin_queue->cq_dma.paddr;
}

static uint16_t nvme_submit_admin_command(nvme_t *nvme, uint8_t opc, nvme_command_t command)
{
    nvme_queue_t *aq = nvme->admin_queue;

    spinlock_acquire(&aq->lock);

    // find free cid
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
        return UINT16_MAX; // no CIDs left
    }

    // check for full SQ (leave one slot empty)
    uint16_t next_tail = (aq->tail + 1) % aq->depth;
    if (next_tail == aq->head)
    {
        aq->cid_used[cid] = false;
        spinlock_release(&aq->lock);
        return UINT16_MAX; // SQ full
    }

    // prepare entry
    nvme_sq_entry_t new_entry = {0};
    new_entry.opc = opc;
    new_entry.cid = cid;
    new_entry.psdt = 0;
    new_entry.command = command;

    aq->sq[aq->tail] = new_entry;

    // increment tail
    aq->tail = next_tail;

    // ring doorbell
    NVME_SQ_TDBL(nvme->registers, aq->qid, nvme->db_stride) = aq->tail;

    spinlock_release(&aq->lock);
    return cid;
}


// waits until command with given cid is completed
// TO-DO: add status, result, flags support
// static void nvme_admin_wait_completion(nvme_t *nvme, uint16_t cid)
// {
//     uint64_t timeout = 1000000;
//     while (timeout--)
//     {
//         nvme_cq_entry_t *entry = nvme_poll_cq(nvme, nvme->admin_queue);
//         if (entry && entry->cid == cid)
//             return;
//     }
//     log(LOG_ERROR, "NVMe admin command CID=%u timed out", cid);
// }

// testing func
static void nvme_admin_wait_completion(nvme_t *nvme, uint16_t cid)
{
    uint64_t timeout = 1000000;
    while (timeout--)
    {
        nvme_cq_entry_t *entry = nvme_poll_cq(nvme, nvme->admin_queue);
        if (entry && entry->cid == cid)
        {
            // Check status
            if (entry->status != 0)
            {
                log(LOG_ERROR, "NVMe command CID=%u failed with status=0x%04X",
                    cid, entry->status);
            }
            return;
        }
        arch_lcpu_relax();  // Add this to avoid busy-waiting
    }
    log(LOG_ERROR, "NVMe admin command CID=%u timed out", cid);
}


// --- ACTUAL COMMANDS ---

static void nvme_identify_controller(nvme_t *nvme)
{
    dma_buf_t id_dma = dma_alloc(4096); // identify controller returns 4096 bytes
    if (!id_dma.vaddr)
    {
        log(LOG_ERROR, "identify controller: dma alloc failed");
        return;
    }
    memset(id_dma.vaddr, 0, 4096);

    nvme_command_t cmd = {0};
    cmd.dptr.prp1 = id_dma.paddr;
    cmd.cdw10 = 1; // CNS=1 for controller identify

    uint16_t cid = nvme_submit_admin_command(nvme, 0x06, cmd); // opcode 0x06 = Identify
    nvme_admin_wait_completion(nvme, cid);

    nvme->identity = (nvme_cid_t *)vm_alloc(sizeof(nvme_cid_t));

    memcpy(nvme->identity, id_dma.vaddr, sizeof(nvme_cid_t));

    dma_free(&id_dma);
}

// dummy funcs
int nvme_read(drive_t *d, const void *buf, uint64_t lba, uint64_t count)
{
    (void)d; (void)buf; (void)lba; (void)count;
    return 0; // just a stub for testing
}

int nvme_write(drive_t *d, const void *buf, uint64_t lba, uint64_t count)
{
    (void)d; (void)buf; (void)lba; (void)count;
    return 0; // stub
}


// --- NAMESPACE SHIT ---

void nvme_namespace_init(nvme_t *nvme, uint32_t nsid, nvme_nsidn_t *nsidnt)
{
    if (nsidnt->nsze == 0) return;

    nvme_namespace_t *ns = vm_alloc(sizeof(*ns));
    if (!ns) return;

    ns->nsid = nsid;
    ns->controller = nvme;

    // parse LBA size and count
    uint8_t flbas_index = nsidnt->flbas & 0X0F;
    uint32_t lbaf       = nsidnt->lbafN[flbas_index];
    uint32_t lba_shift  = (lbaf >> 16) & 0XFF;

    ns->lba_size = 1U << lba_shift;
    ns->lba_count = nsidnt->nsze;

    // TO-DO: add error handling

    drive_t *d = drive_create(DRIVE_TYPE_NVME);
    if (!d) return;

    // set serial number and model
    char sn[21] = { 0 };
    strncpy(sn, nvme->identity->sn, 20);

    char mn[40] = { 0 };
    strncpy(mn, nvme->identity->mn, 40);

    d->serial = strdup(sn);
    d->model = strdup(mn);

    d->sectors = ns->lba_count;
    d->sector_size = ns->lba_size;

    d->read_sectors = nvme_read;

    d->device.driver_data = (void *)ns;

    log(LOG_INFO, "Namespace %u: LBAs=%lu, LBA size=%lu", nsid, nsidnt->nsze, 1UL << ((nsidnt->lbafN[nsidnt->flbas & 0x0F] >> 16) & 0xFF));
    log(LOG_INFO, "Drive Model: %s", d->model);
    log(LOG_INFO, "Drive Serial: %s", d->serial);


    drive_mount(d);
}

static void nvme_identify_namespace(nvme_t *nvme)
{
    ASSERT(nvme);
    ASSERT(nvme->identity);

    uint32_t nn = nvme->identity->nn; // number of namespaces


    log(LOG_INFO, "Controller SN: %.20s", nvme->identity->sn);
    log(LOG_INFO, "Controller Model: %.40s", nvme->identity->mn);
    log(LOG_INFO, "Firmware: %.8s", nvme->identity->fr);
    log(LOG_INFO, "Number of namespaces: %u", nvme->identity->nn);

    for (uint32_t nsid = 1; nsid <= nn; nsid++)
    {
        dma_buf_t ns_dma = dma_alloc(sizeof(nvme_nsidn_t));
        memset(ns_dma.vaddr, 0, sizeof(nvme_nsidn_t));

        nvme_command_t identify_ns =
        {
            .nsid = nsid,
            .dptr.prp1 = ns_dma.paddr,
            .cdw10 = 0X00
        };

        uint16_t cid = nvme_submit_admin_command(nvme, 0x06, identify_ns); // opcode 0x06 = Identify
        nvme_admin_wait_completion(nvme, cid);

        nvme_nsidn_t *nsidnt = (nvme_nsidn_t *)ns_dma.vaddr;

        if (nsidnt->nsze != 0)
            nvme_namespace_init(nvme, nsid, nsidnt);

        dma_free(&ns_dma);
    }
}

// --- INIT ---
static void pci_dump_bars(pci_header_type0_t *h)
{
    // Common fields that matter for BAR decode
    log(LOG_DEBUG, "PCI CMD=%#04x STATUS=%#04x",
        h->common.command, h->common.status);

    for (int i = 0; i < 6; i++)
        log(LOG_DEBUG, "BAR%d raw = %#010x", i, h->bar[i]);

    // Decode BAR0 quickly (typical NVMe)
    uint32_t lo = h->bar[0];
    uint32_t hi = h->bar[1];

    if (lo == 0 && hi == 0)
    {
        log(LOG_WARN, "BAR0/BAR1 are both 0 (BAR likely unassigned)");
        return;
    }

    if (lo & 0x1)
    {
        // I/O BAR (should not happen for NVMe)
        uint32_t io_base = lo & 0xFFFFFFFCu;
        log(LOG_WARN, "BAR0 is I/O BAR: io_base=%#x", io_base);
        return;
    }

    uint32_t type = (lo >> 1) & 0x3; // 0=32-bit, 2=64-bit
    uint32_t prefetch = (lo >> 3) & 0x1;

    if (type == 0x0)
    {
        uint64_t base = (uint64_t)(lo & 0xFFFFFFF0u);
        log(LOG_DEBUG, "BAR0 mem32 base=%#lx prefetch=%u", base, prefetch);
    }
    else if (type == 0x2)
    {
        uint64_t base = ((uint64_t)hi << 32) | (uint64_t)(lo & 0xFFFFFFF0u);
        log(LOG_DEBUG, "BAR0 mem64 base=%#lx prefetch=%u", base, prefetch);
    }
    else
    {
        log(LOG_WARN, "BAR0 mem type=%u (unexpected)", type);
    }
}

void nvme_init(pci_header_type0_t *header)
{
    log(LOG_DEBUG, "Entered nvme init function.");
    pci_dump_bars(header);

    header->common.command |= (1 << 1) | (1 << 2);  // Enable Memory + Bus Master

    nvme_t *nvme = vm_alloc(sizeof(nvme_t));

    // Read the BAR as 64-bit MMIO base
    uint64_t bar0_phys = ((uint64_t)header->bar[1] << 32) | (header->bar[0] & 0xFFFFFFF0u);
    log(LOG_DEBUG, "NVMe BAR0 phys = %#lx", bar0_phys);

    nvme->registers = (nvme_regs_t *)mmio_map((uintptr_t)bar0_phys, 0x4000);
    if (!nvme->registers)
    {
        log(LOG_ERROR, "mmio_map BAR0 failed");
        return;
    }

    nvme_cap_t *cap = (nvme_cap_t *)&nvme->registers->CAP;
    nvme->db_stride = 4 << cap->dstrd;

    log(LOG_DEBUG, "CAP: MQES=%u, TO=%u, DSTRD=%u, CSS=%u",
        cap->mqes, cap->to, cap->dstrd, cap->css);

    volatile uint32_t *cc_reg = (volatile uint32_t *)((uintptr_t)nvme->registers + 0x14); // CC offset
    uint32_t cc_before = *cc_reg;
    log(LOG_DEBUG, "CC raw before reset = %#x", cc_before);

    // Try a no-op write: write same value back
    *cc_reg = cc_before;
    log(LOG_DEBUG, "CC raw after noop write = %#x", *cc_reg);

    // basic flow
    nvme_reset(nvme);
    nvme_create_admin_queue(nvme);
    nvme_start(nvme);
    nvme_identify_controller(nvme);
    nvme_identify_namespace(nvme);
}
