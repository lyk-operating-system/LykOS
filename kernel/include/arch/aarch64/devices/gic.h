#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct
{
    size_t min_global_irq;
    size_t max_global_irq;

    void (*set_base)(uintptr_t gicc_base, uintptr_t gicd_base);

    void (*gicc_init)();
    void (*gicd_init)();

    void (*enable_int)(uint32_t intid);
    void (*disable_int)(uint32_t intid);

    void (*set_target)(uint32_t intid, uint32_t cpu);

    uint32_t (*ack_int)();
    void (*end_of_int)(uint32_t iar);
}
aarch64_gic_t;

extern aarch64_gic_t * aarch64_gic;

void aarch64_gic_detect();
