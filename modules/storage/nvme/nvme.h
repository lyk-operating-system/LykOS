#pragma once

#include "assert.h"
#include "dev/bus/pci.h"
#include "dev/device.h"
#include "hhdm.h"
#include "mm/dma.h"
#include "sync/spinlock.h"
#include <stdint.h>

#define NVME_ADMIN_QUEUE_DEPTH 64
#define NVME_IO_QUEUE_DEPTH    64

#define NVME_CQE_PHASE(e)   ((uint16_t)((e)->status & 1u))
#define NVME_CQE_STATUS(e)  ((uint16_t)((e)->status >> 1))

// --- ID STRUCTS ---
// NVMe Identify Controller data struct
/* source: https://nvmexpress.org/wp-content/uploads/NVM-Express-Base-Specification-Revision-2.3-2025.08.01-Ratified.pdf
   Figure 328 */
// ------------
 typedef struct
 {
     // controller capabilities and general info
     uint16_t vid;              // pci vendor id
     uint16_t ssvid;            // pci subsystem vendor id
     char     sn[20];           // serial number (ascii)
     char     mn[40];           // model number (ascii)
     char     fr[8];            // firmware revision (ascii)
     uint8_t  rab;              // recommended arbitration burst
     uint8_t  ieee[3];          // ieee oui identifier
     uint8_t  cmic;             // multi-path i/o and namespace sharing stuff
     uint8_t  mdts;             // max data transfer size
     uint16_t cntlid;           // controller id
     uint32_t ver;              // version
     uint32_t rtd3r;            // rtd3 resume latency
     uint32_t rtd3e;            // rtd3 entry latency
     uint32_t oaes;             // optional async events supported
     uint32_t ctratt;           // controller attributes
     uint16_t rrls;             // read recovery levels supported
     uint8_t  _rsvd66[9];       // reserved
     uint8_t  cntrltype;        // controller type
     uint8_t  fguid[16];        // fru globally unique id
     uint16_t crdt1;            // command retry delay time 1
     uint16_t crdt2;            // command retry delay time 2
     uint16_t crdt3;            // command retry delay time 3
     uint8_t  _rsvd86[106];     // reserved
     uint8_t  _rsvd_mi[16];     // reserved for nvme management interface

     // admin command set attributes
     uint16_t oacs;             // optional admin command support
     uint8_t  acl;              // abort command limit
     uint8_t  aerl;             // async event request limit
     uint8_t  frmw;             // firmware update support
     uint8_t  lpa;              // log page attributes
     uint8_t  elpe;             // error log page entries
     uint8_t  npss;             // number of power states supported
     uint8_t  avscc;            // admin vendor-specific command config
     uint8_t  apsta;            // autonomous power state transitions
     uint16_t wctemp;           // warning composite temp threshold
     uint16_t cctemp;           // critical composite temp threshold
     uint16_t mtfa;             // max time for firmware activation
     uint32_t hmpre;            // host memory buffer preferred size
     uint32_t hmmin;            // host memory buffer minimum size
     uint8_t  tnvmcap[16];      // total nvm capacity (128-bit)
     uint8_t  unvmcap[16];      // unallocated nvm capacity (128-bit)
     uint32_t rpmbs;            // replay protected memory block support
     uint16_t edstt;            // extended device self-test time
     uint8_t  dsto;             // device self-test options
     uint8_t  fwug;             // firmware update granularity
     uint16_t kas;              // keep alive support
     uint16_t hctma;            // host controlled thermal management
     uint16_t mntmt;            // min thermal management temp
     uint16_t mxtmt;            // max thermal management temp
     uint32_t sanicap;          // sanitize capabilities
     uint32_t hmminds;          // host memory buffer min descriptor size
     uint16_t hmmaxd;           // host memory max descriptor entries
     uint16_t nsetidmax;        // max nvm set identifier
     uint16_t endgidmax;        // max endurance group identifier
     uint8_t  anatt;            // ana transition time
     uint8_t  anacap;           // ana capabilities
     uint32_t anagrpmax;        // max ana group identifier
     uint32_t nanagrpid;        // number of ana group identifiers
     uint32_t pels;             // persistent event log size
     uint8_t  _rsvd164[156];    // reserved

     // nvm command set attributes
     uint8_t  sqes;             // submission queue entry size
     uint8_t  cqes;             // completion queue entry size
     uint16_t maxcmd;           // max outstanding commands
     uint32_t nn;               // number of namespaces
     uint16_t oncs;             // optional nvm command support
     uint16_t fuses;            // fused operation support
     uint8_t  fna;              // format nvm attributes
     uint8_t  vwc;              // volatile write cache support
     uint16_t awun;             // atomic write unit (normal)
     uint16_t awupf;            // atomic write unit (power fail)
     uint8_t  nvscc;            // nvm vendor-specific command config
     uint8_t  nwpc;             // namespace write protection capabilities
     uint16_t acwu;             // atomic compare & write unit
     uint16_t _rsvd214;         // reserved
     uint32_t sgls;             // sgl support
     uint32_t mnan;             // max allowed namespaces
     uint8_t  _rsvd220[224];    // reserved

     // reserved for i/o command set specific data
     uint8_t  _rsvd_iocs[1344]; // reserved

     // power state descriptors (32 entries, 32 bytes each)
     uint8_t  psd[1024];        // power state descriptors

     // vendor-specific area
     uint8_t  vs[1024];         // vendor specific
 }
 __attribute__((packed))
 nvme_cid_t;

 // NVMe Identify Namespace data struct
 /* source: https://nvmexpress.org/wp-content/uploads/NVM-Express-Base-Specification-Revision-2.3-2025.08.01-Ratified.pdf
    Figure 335 */
 // ------------
 typedef struct
 {
     uint64_t nsze;             // namespace size (in lbas)
     uint64_t ncap;             // namespace capacity (in lbas)
     uint64_t nuse;             // namespace utilization (in lbas)
     uint8_t  nsfeat;           // namespace features
     uint8_t  nlbaf;            // number of lba formats (0-based)
     uint8_t  flbas;            // currently formatted lba size
     uint8_t  mc;               // metadata capabilities
     uint8_t  dpc;              // end-to-end data protection capabilities
     uint8_t  dps;              // active data protection settings
     uint8_t  nmic;             // multi-path i/o and sharing support
     uint8_t  rescap;           // reservation capabilities
     uint8_t  fpi;              // format progress indicator
     uint8_t  dlfeat;           // deallocate logical block features
     uint16_t nawun;            // atomic write unit (normal)
     uint16_t nawupf;           // atomic write unit (power fail)
     uint16_t nacwu;            // atomic compare & write unit
     uint16_t nabsn;            // atomic boundary size (normal)
     uint16_t nabo;             // atomic boundary offset
     uint16_t nabspf;           // atomic boundary size (power fail)
     uint16_t noiob;            // optimal i/o boundary
     uint64_t nvmcap[2];        // nvm capacity (128-bit)
     uint16_t npwg;             // preferred write granularity
     uint16_t npwa;             // preferred write alignment
     uint16_t npdg;             // preferred deallocate granularity
     uint16_t npda;             // preferred deallocate alignment
     uint16_t nows;             // optimal write size
     uint16_t mssrl;            // max single source range length
     uint32_t mcl;              // max copy length
     uint8_t  msrc;             // max source range count
     uint8_t  _rsvd81[11];      // reserved
     uint32_t anagrpid;         // ana group identifier
     uint8_t  _rsvd96[3];       // reserved
     uint8_t  nsattr;           // namespace attributes
     uint16_t nvmsetid;         // nvm set identifier
     uint16_t endgid;           // endurance group identifier
     uint64_t nguid[2];         // namespace globally unique id (128-bit)
     uint64_t eui64;            // ieee extended unique identifier

     // lba format support (16 entries, 4 bytes each)
     // bits 0-15: metadata size
     // bits 16-23: lba data size (2^lbads)
     // bits 24-25: relative performance
     uint32_t lbaf[16];         // lba format support

     uint8_t  _rsvd192[192];    // reserved
     uint8_t  vs[3712];         // vendor specific
 }
 __attribute__((packed))
 nvme_nsidn_t;
static_assert(sizeof(nvme_nsidn_t) == 4096);

// Register stuff
/* source: https://nvmexpress.org/wp-content/uploads/NVM-Express-Base-Specification-Revision-2.3-2025.08.01-Ratified.pdf
   Figure 33 */
// ------------

// NVMe Registers (main controller structure)
typedef volatile struct
{
    uint64_t CAP;          // controller capabilities
    uint32_t VS;           // version
    uint32_t INTMS;        // interrupt mask set
    uint32_t INTMC;        // interrupt mask clear
    uint32_t CC;           // controller configuration
    uint32_t _rsvd0;
    uint32_t CSTS;         //controller status
    uint32_t _rsvd1;
    uint32_t AQA;          // admin queue attributes
    uint64_t ASQ;          // admin submission queue base address
    uint64_t ACQ;          // admin completion queue base address
    uint8_t  _rsvd2[0x1000 - 0x38];
}
__attribute__((packed))
nvme_regs_t;

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
    uint16_t status;
}
__attribute__((packed))
nvme_cq_entry_t;

static_assert(sizeof(nvme_cq_entry_t) == 16);
// -----

// Queue
typedef struct
{
    nvme_sq_entry_t *sq;
    volatile nvme_cq_entry_t *cq;

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

    uint32_t db_stride;
    uint16_t next_qid;
}
__attribute__((packed))
nvme_t;

// NVMe namespace
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

void nvme_init(volatile pci_header_type0_t *header);
