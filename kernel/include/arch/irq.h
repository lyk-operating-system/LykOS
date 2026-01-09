#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct
{
    uint32_t target_cpuid;
    uint32_t vector; // Interrupt number
}
irq_handle_t;

typedef void (*irq_handler_fn)(irq_handle_t handle, void *context);

[[nodiscard]] bool arch_irq_alloc(
    irq_handler_fn handler,
    void *context,
    uint32_t target_cpuid,
    irq_handle_t *out_irq_handle
);
void arch_irq_free(irq_handle_t handle);
