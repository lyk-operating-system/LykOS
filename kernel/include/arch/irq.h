#pragma once

#include <stddef.h>
#include <stdint.h>

[[nodiscard]] bool arch_irq_alloc(
    void (*handler)(uint32_t irq, void *context),
    void *context,
    uint32_t target_cpuid,
    uint32_t *out_irq
);

void arch_irq_free(uint32_t irq);

void arch_irq_enable(uint32_t irq);
void arch_irq_disable(uint32_t irq);

void arch_irq_dispatch(uint32_t irq);
