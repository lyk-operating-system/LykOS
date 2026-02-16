#include "arch/x86_64/tables/gdt.h"

#include "log.h"
#include "mm/mm.h"
#include "proc/sched.h"

#include <stdint.h>

// Must be set to 1 for any valid segment.
#define ACCESS_PRESENT (1 << 7)
// Descriptor privilege level field. Contains the CPU Privilege level of the
// segment.
#define ACCESS_DPL(DPL) (((DPL) & 0b11) << 5)
#define ACCESS_TYPE_TSS (0b1001)
#define ACCESS_TYPE_CODE(CONFORM, READ) ((1 << 4) | (1 << 3) | ((CONFORM) << 2) | ((READ) << 1))
#define ACCESS_TYPE_DATA(DIRECTION, WRITE) ((1 << 4) | ((DIRECTION) << 2) | ((WRITE) << 1))

#define FLAG_GRANULARITY (1 << 7)
#define FLAG_DB (1 << 6)
#define FLAG_LONG (1 << 5)
#define FLAG_SYSTEM_AVL (1 << 4)

typedef struct
{
    uint16_t limit;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  flags;
    uint8_t  base_high;
}
__attribute__((packed))
gdt_entry_t;

typedef struct
{
    gdt_entry_t gdt_entry;
    uint32_t base_ext;
    uint32_t _rsv;
}
__attribute__((packed))
gdt_system_entry_t;

typedef struct
{
    uint16_t limit;
    uint64_t base;
}
__attribute__((packed))
gdtr_t;

extern void __gdt_load(gdtr_t *gdtr, uint64_t selector_code, uint64_t selector_data);

static gdt_entry_t g_gdt[] = {
    // Null descriptor.
    {},
    // Kernel code.
    {
        .limit = 0,
        .base_low = 0,
        .base_mid = 0,
        .access = ACCESS_PRESENT | ACCESS_DPL(0) | ACCESS_TYPE_CODE(0, 1),
        .flags = FLAG_LONG,
        .base_high = 0
    },
    // Kernel data.
    {
        .limit = 0,
        .base_low = 0,
        .base_mid = 0,
        .access = ACCESS_PRESENT | ACCESS_DPL(0) | ACCESS_TYPE_DATA(0, 1),
        .flags = 0,
        .base_high = 0
    },
    // User data.
    {
        .limit = 0,
        .base_low = 0,
        .base_mid = 0,
        .access = ACCESS_PRESENT | ACCESS_DPL(3) | ACCESS_TYPE_DATA(0, 1),
        .flags = 0,
        .base_high = 0
    },
    // User code.
    {
        .limit = 0,
        .base_low = 0,
        .base_mid = 0,
        .access = ACCESS_PRESENT | ACCESS_DPL(3) | ACCESS_TYPE_CODE(0, 1),
        .flags = FLAG_LONG,
        .base_high = 0},
    // TSS.
    {},
    {}
};

//

static void load_tss(x86_64_tss_t *tss)
{
    memset(tss, 0, sizeof(x86_64_tss_t));

    uint16_t tss_segment = sizeof(g_gdt) - 16;

    gdt_system_entry_t *sys_entry = (gdt_system_entry_t *)((uintptr_t)g_gdt + tss_segment);
    *sys_entry = (gdt_system_entry_t) {
        .gdt_entry = (gdt_entry_t) {
            .access    = ACCESS_PRESENT | ACCESS_TYPE_TSS,
            .flags     = FLAG_SYSTEM_AVL | ((sizeof(x86_64_tss_t) >> 16) & 0b1111),
            .limit     = (uint16_t)sizeof(x86_64_tss_t),
            .base_low  = (uint16_t)(uint64_t)tss,
            .base_mid  = (uint8_t)((uint64_t)tss >> 16),
            .base_high = (uint8_t)((uint64_t)tss >> 24),
        },
        .base_ext = (uint32_t)((uint64_t)tss >> 32)
    };

    asm volatile("ltr %0" : : "m"(tss_segment));
}


void x86_64_gdt_init_cpu()
{
    gdtr_t gdtr = (gdtr_t){
        .limit = sizeof(g_gdt) - 1,
        .base = (uint64_t)&g_gdt
    };

    __gdt_load(&gdtr, GDT_SELECTOR_CODE64_RING0, GDT_SELECTOR_DATA64_RING0);\
    log(LOG_DEBUG, "GDT loaded");

    load_tss(&x86_64_tss[sched_get_curr_cpuid()]);
    log(LOG_DEBUG, "TSS loaded");
}
