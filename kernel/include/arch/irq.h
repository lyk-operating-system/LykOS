#pragma once

#include <stdint.h>

typedef enum
{
    INT_TYPE_GLOBAL,  // AArch64 - SPI / X86_64 - IOAPIC lines
    INT_TYPE_PER_CPU, // AArch64 - PPI / x86_64 - LAPIC LVT
    INT_TYPE_MSI,     // PCIe - Message Signaled Interrupts
    INT_TYPE_DYNAMIC, // X86_64 - use any available vector
}
arch_int_type_t;

typedef enum
{
    INT_TRIGGER_EDGE_RISING,
    INT_TRIGGER_EDGE_FALLING,
    INT_TRIGGER_LEVEL_HIGH,
    INT_TRIGGER_LEVEL_LOW
}
arch_int_trigger_t;

typedef void (*irq_handler_t)(void* data);

typedef struct
{
    uint32_t irq_id;  // The global identifier
    uint8_t vector;   // Architecture specific (eg x86 IDT index)
    arch_int_type_t type;
    void *data;       // Private data passed to the handler.
}
arch_interrupt_t;

/**
 * @brief Allocates and registers an interrupt.
 * @param name String name for debugging/logging.
 * @return interrupt_t* Pointer to the handle, or NULL on failure.
 */
arch_interrupt_t* arch_int_request(
    arch_int_type_t type,
    arch_int_trigger_t trigger,
    irq_handler_t handler,
    const char* name,
    void* data
);

/**
 * @brief Releases the interrupt and clears the vector/ID mapping.
 */
bool arch_int_free(arch_interrupt_t *interrupt);

/**
 * @brief Enables the interrupt at the controller level.
 */
void arch_int_enable(arch_interrupt_t *interrupt);

/**
 * @brief Disables the interrupt at the controller level.
 */
void arch_int_disable(arch_interrupt_t *interrupt);
