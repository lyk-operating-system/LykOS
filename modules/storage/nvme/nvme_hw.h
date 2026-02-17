#pragma once
#include <stdint.h>

// Doorbell Macros
#define NVME_SQ_TDBL(base, qid, stride) \
    (*(volatile uint32_t *)((uintptr_t)(base) + 0x1000 + (2 * (qid)) * (stride)))

#define NVME_CQ_HDBL(base, qid, stride) \
    (*(volatile uint32_t *)((uintptr_t)(base) + 0x1000 + (2 * (qid) + 1) * (stride)))

// CAP masks/shifts
#define NVME_CAP_MQES_MASK   0x000000000000FFFFull
#define NVME_CAP_TO_SHIFT    24
#define NVME_CAP_TO_MASK     0x00000000FF000000ull
#define NVME_CAP_DSTRD_SHIFT 32
#define NVME_CAP_DSTRD_MASK  0x0000000F00000000ull
#define NVME_CAP_CSS_SHIFT   37
#define NVME_CAP_CSS_MASK    0x00001FE000000000ull

// CAP extractors
#define NVME_CAP_MQES(cap)   ((uint16_t)((cap) & NVME_CAP_MQES_MASK))
#define NVME_CAP_TO(cap)     ((uint8_t)(((cap) & NVME_CAP_TO_MASK) >> NVME_CAP_TO_SHIFT))
#define NVME_CAP_DSTRD(cap)  ((uint8_t)(((cap) & NVME_CAP_DSTRD_MASK) >> NVME_CAP_DSTRD_SHIFT))
#define NVME_CAP_CSS(cap)    ((uint16_t)(((cap) & NVME_CAP_CSS_MASK) >> NVME_CAP_CSS_SHIFT))

// CC bits
#define NVME_CC_EN             (1u << 0)
#define NVME_CC_SHN_SHIFT      2
#define NVME_CC_AMS_SHIFT      11
#define NVME_CC_MPS_SHIFT      7
#define NVME_CC_CSS_SHIFT      4
#define NVME_CC_IOSQES_SHIFT   16
#define NVME_CC_IOCQES_SHIFT   20

// CSTS bits
#define NVME_CSTS_RDY          (1u << 0)
#define NVME_CSTS_SHST_SHIFT   2
#define NVME_CSTS_SHST_MASK    (3u << NVME_CSTS_SHST_SHIFT)

// TO-DO: Add opcode values

// --- READ/WRITE HELPERS ---

static inline uint32_t mmio_read32(volatile void *base)
{
    return *(volatile uint32_t *)base;
}

static inline void mmio_write32(volatile void *base, uint32_t v)
{
    *(volatile uint32_t *)base = v;
}

static inline uint64_t mmio_read64(volatile void *base)
{
    return *(volatile uint64_t *)base;
}

static inline void mmio_write64(volatile void *base, uint64_t v)
{
    *(volatile uint64_t *)base = v;
}
