#include "nvme.h"
#include "dev/storage/drive.h"
#include "mm/heap.h"
#include "dev/bus/pci.h"
#include "mm/mm.h"
#include "utils/string.h"
#include <stdint.h>
#include <sys/types.h>

// --- HELPERS ---

// poll the completion queue once; returns pointer to a valid completed entry or NULL
static nvme_cq_entry_t *nvme_poll_cq(nvme_t *nvme, nvme_queue_t *queue)
{
    nvme_cq_entry_t *entry = &queue->cq[queue->head];

    if ((entry->phase & 1) != queue->phase)
        return NULL; // nothing new

    // got a new entry
    queue->head = (queue->head + 1) % queue->depth;
    if (queue->head == 0)
        queue->phase ^= 1;

    // tell controller entry is consumed
    NVME_CQ_HDBL(nvme->registers, queue->qid, nvme->registers->stride) = queue->head;

    return entry;
}

// set ready status and wait
static void nvme_wait_ready(nvme_t *nvme, bool ready)
{
    while (nvme->registers->CSTS.rdy != ready)
        ; // spin
}

// --- BASIC FUNCS ---
void nvme_reset(nvme_t *nvme)
{
    if(nvme->registers->CC.en)
        while(nvme->registers->CSTS.rdy)
            ;
    nvme->registers->CC.en = 0;
}

void nvme_start(nvme_t *nvme)
{
    nvme->registers->CC.ams = 0;
    nvme->registers->CC.mps = 0; // 4kb page shift
    nvme->registers->CC.css = 0;

    // set queue entry sizes
    nvme->registers->CC.iosqes = 6;
    nvme->registers->CC.iocqes = 4;

    nvme->registers->CC.en  = 1;
}

// --- ADMIN FUNCS ---
// TO-DO: add error handling
static void nvme_create_admin_queue(nvme_t *nvme)
{
    size_t sq_size = NVME_ADMIN_QUEUE_DEPTH * sizeof(nvme_sq_entry_t);
    size_t cq_size = NVME_ADMIN_QUEUE_DEPTH * sizeof(nvme_cq_entry_t);

    nvme_queue_t *aq = heap_alloc(sizeof(nvme_queue_t));
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

    // set queue sizes in AQA register
    nvme->registers->AQA.asqs = NVME_ADMIN_QUEUE_DEPTH - 1;
    nvme->registers->AQA.acqs = NVME_ADMIN_QUEUE_DEPTH - 1;

    // program controller registers with physical addresses
    nvme->registers->ASQ = dma_phys_addr(nvme->admin_queue->sq);
    nvme->registers->ACQ = dma_phys_addr(nvme->admin_queue->cq);
}

static uint16_t nvme_submit_admin_command(nvme_t *nvme, uint8_t opc, uint16_t cid, nvme_command_t command)
{
    nvme_queue_t *aq = nvme->admin_queue;

    nvme_sq_entry_t new_entry =
    {
        .opc = opc,
        .cid = cid,
        .command = command,
    };
    aq->sq[aq->tail] = new_entry;

    // increment
    aq->tail = (aq->tail + 1) % aq->depth;

    // ring doorbell
    NVME_SQ_TDBL(nvme->registers, aq->qid, nvme->registers->stride) = aq->tail;

    return cid;
}

// waits until command with given cid is completed
// TO-DO: add status, result, flags support
static void nvme_admin_wait_completion(nvme_t *nvme, uint16_t cid)
{
    while (true)
    {
        nvme_cq_entry_t *entry = nvme_poll_cq(nvme, nvme->admin_queue);
        if (entry && entry->cid == cid)
            return;
    }
}

// --- ACTUAL COMMANDS ---

static void nvme_identify_controller(nvme_t *nvme)
{
    nvme->identity = (nvme_cid_t *)dma_map(sizeof(nvme_cid_t));
    memset((void *)nvme->identity, 0, sizeof(nvme_cid_t));

    nvme_command_t cmd = {0};
    cmd.dptr.prp1 = dma_phys_addr((void *)nvme->identity);
    cmd.cdw10 = 1; // CNS=1 for controller identify

    nvme_submit_admin_command(nvme, 0x06, 1, cmd); // opcode 0x06 = Identify
    nvme_admin_wait_completion(nvme, 1);
}

static void nvme_identify_namespace(nvme_t *nvme)
{
    nvme->
}

int nvme_read(drive_t *d, const void *buf, uint64_t lba, uint64_t count)
{

}

// --- INIT ---

void nvme_namespace_init(nvme_t *nvme, uint32_t nsid, nvme_nsidn_t *nsidnt)
{
    if (nsidnt->nsze == 0) return;

    nvme_namespace_t *ns = heap_alloc(sizeof(*ns));
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

    // d->driver = (void *)ns;
    drive_mount(d);

}

void nvme_init(pci_header_type0_t *header)
{
    nvme_t *nvme = heap_alloc(sizeof(nvme_t));

    header->common.command |= (1 << 1);
    nvme->registers = (nvme_regs_t *)(HHDM + (header->bar[0] & ~0xF));

    nvme_cap_t *cap = (nvme_cap_t *)&nvme->registers->CAP;
    nvme->registers->stride = 4 << cap->dstrd;

    // basic flow
    nvme_reset(nvme);
    nvme_wait_ready(nvme, 0);
    nvme_create_admin_queue(nvme);
    nvme_start(nvme);
    nvme_wait_ready(nvme, 1);
    nvme_identify_controller(nvme);
}
