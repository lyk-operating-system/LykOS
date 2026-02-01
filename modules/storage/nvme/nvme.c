#define LOG_PREFIX "NVME"
#include "nvme.h"

#include "dev/storage/drive.h"
#include "log.h"
#include "dev/bus/pci.h"
#include "mm/mm.h"
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
    NVME_CQ_HDBL(nvme->registers, queue->qid, nvme->registers->stride) = queue->head;

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

    // Disable the controller
    nvme->registers->CC.en = 0;

    // Wait for controller to acknowledge disable (CSTS.RDY = 0)
    uint64_t timeout = 1000000;
    while (nvme->registers->CSTS.rdy && timeout--)
        arch_lcpu_relax();

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

    nvme->admin_queue->sq = (nvme_sq_entry_t *) dma_map(sq_size);
    nvme->admin_queue->cq = (nvme_cq_entry_t *) dma_map(cq_size);

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
    nvme->registers->ASQ = dma_phys_addr(nvme->admin_queue->sq);
    nvme->registers->ACQ = dma_phys_addr(nvme->admin_queue->cq);
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
    NVME_SQ_TDBL(nvme->registers, aq->qid, nvme->registers->stride) = aq->tail;

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
    nvme->identity = (nvme_cid_t *)dma_map(sizeof(nvme_cid_t));
    memset((void *)nvme->identity, 0, sizeof(nvme_cid_t));

    nvme_command_t cmd = {0};
    cmd.dptr.prp1 = dma_phys_addr((void *)nvme->identity);
    cmd.cdw10 = 1; // CNS=1 for controller identify

    uint16_t cid = nvme_submit_admin_command(nvme, 0x06, cmd); // opcode 0x06 = Identify
    nvme_admin_wait_completion(nvme, cid);
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
        nvme_nsidn_t *nsidnt = (nvme_nsidn_t *)dma_map(sizeof(nvme_nsidn_t));
        if (!nsidnt) continue;

        memset((void *)nsidnt, 0, sizeof(nvme_nsidn_t));

        nvme_command_t identify_ns =
        {
            .nsid = nsid,
            .dptr.prp1 = dma_phys_addr(nsidnt),
            .cdw10 = 0X00
        };

        uint16_t cid = nvme_submit_admin_command(nvme, 0x06, identify_ns); // opcode 0x06 = Identify
        nvme_admin_wait_completion(nvme, cid);

        if (nsidnt->nsze != 0)
            nvme_namespace_init(nvme, nsid, nsidnt);

        dma_unmap((uintptr_t) nsidnt, sizeof(nvme_nsidn_t));
    }
}

// --- INIT ---

void nvme_init(pci_header_type0_t *header)
{
    log(LOG_DEBUG, "Entered nvme init function.");

    // TEMP FIX: Manually assign BAR
    header->bar[0] = 0xFEBF0004;  // Physical addr + 64-bit flag
    header->bar[1] = 0x00000000;   // Upper 32 bits (zero for addresses < 4GB)
    header->common.command |= (1 << 1) | (1 << 2);  // Enable Memory + Bus Master

    nvme_t *nvme = vm_alloc(sizeof(nvme_t));

    // Read the BAR we just assigned
    uint64_t bar0 = ((uint64_t)header->bar[1] << 32) | (header->bar[0] & 0xFFFFFFF0);
    nvme->registers = (nvme_regs_t *)(HHDM + bar0);

    nvme_cap_t *cap = (nvme_cap_t *)&nvme->registers->CAP;
    nvme->registers->stride = 4 << cap->dstrd;

    log(LOG_DEBUG, "CAP: MQES=%u, TO=%u, DSTRD=%u, CSS=%u",
        cap->mqes, cap->to, cap->dstrd, cap->css);

    // basic flow
    nvme_reset(nvme);
    nvme_create_admin_queue(nvme);
    nvme_start(nvme);
    nvme_identify_controller(nvme);
    nvme_identify_namespace(nvme);
}
