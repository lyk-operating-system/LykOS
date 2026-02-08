#pragma once

#include "assert.h"
#include "dev/bus/pci.h"
#include "dev/device.h"
#include "mm/dma.h"
#include "mm/vm.h"
#include "hhdm.h"
#include "sync/spinlock.h"
#include "dev/storage/drive.h"
#include "utils/string.h"

#define NVME_ADMIN_QUEUE_DEPTH 64

// Doorbell Macros
#define NVME_SQ_TDBL(base, qid, stride) \
    (*(volatile uint32_t *)((uintptr_t)(base) + 0x1000 + (2 * (qid)) * (stride)))

#define NVME_CQ_HDBL(base, qid, stride) \
    (*(volatile uint32_t *)((uintptr_t)(base) + 0x1000 + (2 * (qid) + 1) * (stride)))

// --- ID STRUCTS ---
// controller identification struct
typedef struct
{
    uint16_t vid; // PCI vendor id
    uint16_t ssvid; // subsystem vendor id

    char     sn[20]; // serial number
    char     mn[40]; // model number
    char     fr[8]; // firmware version

    uint8_t  rab; // Recommended Arbitration Burst
    uint8_t  ieee[3]; // IEEE OUI
    uint8_t  cmic; // Controller Multi-Interface Capabilities
    uint8_t  mdts; // Maximum Data Transfer Size

    uint16_t cntlid; // Controller ID
    uint32_t ver; // version

    uint8_t  sqes; // submission queue entry size
    uint8_t  cqes; // completion queue entry size

    uint16_t maxcmd; // maximum commands supported
    uint32_t nn; // number of namespaces
}
__attribute__((packed))
nvme_cid_t;

typedef struct
{
	uint64_t nsze;
	uint64_t ncap;
	uint64_t nuse;
	uint8_t nsfeat;
	uint8_t nlbaf;
	uint8_t flbas;
	uint8_t mc;
	uint8_t dpc;
	uint8_t dps;
	uint8_t nmic;
	uint8_t rescap;
	uint8_t fpi;
	uint8_t dlfeat;
	uint16_t nawun;
	uint16_t nawupf;
	uint16_t nacwu;
	uint16_t nabsn;
	uint16_t nabo;
	uint16_t nabspf;
	uint16_t noiob;
	uint64_t nvmcap[2];
	uint16_t npwg;
	uint16_t npwa;
	uint16_t npdg;
	uint16_t npda;
	uint16_t nows;
	uint16_t mssrl;
	uint32_t mcl;
	uint8_t msrc;
	uint8_t __reserved0[11];
	uint32_t adagrpid;
	uint8_t __reserved1[3];
	uint8_t nsattr;
	uint16_t nvmsetid;
	uint16_t endgid;
	uint64_t nguid[2];
	uint64_t eui64;
	uint32_t lbafN[64];
	uint8_t vendor_specific[3712];
}
__attribute__((packed))
nvme_nsidn_t;
static_assert(sizeof(nvme_nsidn_t) == 4096);

// Register stuff
/* source: https://nvmexpress.org/wp-content/uploads/NVM-Express-Base-Specification-Revision-2.3-2025.08.01-Ratified.pdf
   pg. 78 */
// ------------

// CAP Register
typedef struct
{
    uint64_t mqes     : 16; // Maximum Queue Entries Supported
    uint64_t cqr      : 1;  // Contiguous Queues Required
    uint64_t ams      : 2;  // Arbitration Mechanism Supported
    uint64_t _rsv0    : 5;
    uint64_t to       : 8;  // Timeout
    uint64_t dstrd    : 4;  // Doorbell Stride
    uint64_t nssrs    : 1;  // NVM Subsystem Reset Supported
    uint64_t css      : 8;  // Command Set Supported
    uint64_t bps      : 1;  // Boot Partition Support
    uint64_t cps      : 2;  // Command Formats Supported
    uint64_t mpsmin   : 4;  // Minimum Memory Page Size
    uint64_t mpsmax   : 4;  // Maximum Memory Page Size
    uint64_t pmrs     : 1;  // Persistent Memory Region Support
    uint64_t cmbs     : 1;  // Controller Memory Buffer Support
    uint64_t nsss     : 1;  // Namespace Soft Reset Support
    uint64_t crms     : 2;  // Controller Ready Multi-Status
    uint64_t nsses    : 1;  // NVM Subsystem Supported
} __attribute__((packed)) nvme_cap_t;

// CC Register (Controller Configuration)
typedef struct
{
    uint32_t en       : 1;  // Enable
    uint32_t cfs      : 1;  // Controller Fatal Status
    uint32_t shn      : 2;  // Shutdown Notification
    uint32_t iosqes   : 4;  // I/O Submission Queue Entry Size
    uint32_t iocqes   : 4;  // I/O Completion Queue Entry Size
    uint32_t ams      : 3;  // Arbitration Mechanism Selected
    uint32_t mps      : 4;  // Memory Page Size
    uint32_t css      : 3;  // Command Set Selected
    uint32_t _rsv0    : 10;
} __attribute__((packed)) nvme_cc_t;

// CSTS Register (Controller Status)
typedef struct
{
    uint32_t rdy      : 1;  // Ready
    uint32_t cfs      : 1;  // Controller Fatal Status
    uint32_t shst     : 2;  // Shutdown Status
    uint32_t nssro    : 1;  // NVM Subsystem Reset Occurred
    uint32_t _rsv0    : 27;
} __attribute__((packed)) nvme_csts_t;

// AQA Register (Admin Queue Attributes)
typedef struct
{
    uint32_t asqs     : 12; // Admin Submission Queue Size
    uint32_t _rsv0    : 4;
    uint32_t acqs     : 12; // Admin Completion Queue Size
    uint32_t _rsv1    : 4;
} __attribute__((packed)) nvme_aqa_t;

// NVMe Registers (main controller structure)
typedef volatile struct
{
    uint64_t CAP;          // Controller Capabilities
    uint32_t VS;           // Version
    uint32_t INTMS;        // Interrupt Mask Set
    uint32_t INTMC;        // Interrupt Mask Clear
    nvme_cc_t CC;          // Controller Configuration
    uint32_t _rsvd0;
    nvme_csts_t CSTS;      // Controller Status
    uint32_t _rsvd1;
    nvme_aqa_t AQA;        // Admin Queue Attributes
    uint64_t ASQ;          // Admin Submission Queue Base Address
    uint64_t ACQ;          // Admin Completion Queue Base Address
    uint8_t  _rsvd2[0x1000 - 0x38];
    uint32_t stride;       // Doorbell registers
} __attribute__((packed)) nvme_regs_t;

// ---------

typedef struct
{
    union
    {
        struct
        {
            uint64_t prp1;
            uint64_t prp2;
        };

        uint8_t sgl1[16];
    };
}
__attribute__((packed))
nvme_data_pointer_t;

typedef struct
{
    uint32_t nsid;

    // Reserved
    uint32_t cdw2;
    uint32_t cdw3;

    uint64_t mptr;
    nvme_data_pointer_t dptr;

    // Command specific
    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
}
__attribute__((packed))
nvme_command_t;

static_assert(sizeof(nvme_command_t) == 15 * sizeof(uint32_t));

// --- QUEUE ENTRIES ---
// Submission queue entry
typedef struct
{
    uint8_t opc;
    uint8_t fuse : 2;
    uint8_t _rsv : 4;
    uint8_t psdt : 2;

    uint16_t cid;

    nvme_command_t command;
}
__attribute__((packed))
nvme_sq_entry_t;

static_assert(sizeof(nvme_sq_entry_t) == 64);

// Completion queue entry
typedef struct
{
    uint32_t cdw0;
    uint32_t cdw1;

    uint16_t sq_head;
    uint16_t sq_id;

    uint16_t cid;
    uint16_t phase : 1;
    uint16_t status : 15;
}
__attribute__((packed))
nvme_cq_entry_t;

static_assert(sizeof(nvme_cq_entry_t) == 16);
// -----

// Queue
typedef struct
{
    nvme_sq_entry_t *sq;
    nvme_cq_entry_t *cq;

    dma_buf_t sq_dma;
    dma_buf_t cq_dma;

    uint16_t qid;
    uint16_t depth;
    uint16_t head;
    uint16_t tail;
    uint8_t phase;

    // add these for proper cid allocation
    uint16_t next_cid;
    bool cid_used[NVME_ADMIN_QUEUE_DEPTH];

    spinlock_t lock;
}
__attribute__((packed))
nvme_queue_t;

// NVMe controller
typedef struct
{
    nvme_regs_t *registers;
    pci_header_type0_t dev;

    nvme_queue_t *admin_queue;
    nvme_queue_t *io_queue;

    nvme_cid_t *identity;
}
__attribute__((packed))
nvme_t;

typedef struct
{
    nvme_t* controller;
    uint32_t nsid;

    uint64_t lba_count;
    uint32_t lba_size;
}
__attribute__((packed))
nvme_namespace_t;

// --- FUNCTIONS ---
void nvme_reset(nvme_t *nvme);
void nvme_start(nvme_t *nvme);

void nvme_init(pci_header_type0_t *header);
