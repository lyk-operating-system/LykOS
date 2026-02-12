#pragma once

typedef struct irq irq_t;

typedef enum
{
    IRQ_TRIGGER_EDGE_RISING,
    IRQ_TRIGGER_EDGE_FALLING,
    IRQ_TRIGGER_LEVEL_HIGH,
    IRQ_TRIGGER_LEVEL_LOW,
}
irq_trigger_t;

typedef bool (*irq_handler_t)(irq_t *irq, void *data);

//

#if defined (__x86_64__)

irq_t *irq_claim_legacy(unsigned gsi,
                        irq_trigger_t trigger,
                        irq_handler_t handler,
                        unsigned flags);

#endif

irq_t *irq_alloc(irq_trigger_t trigger,
                 irq_handler_t handler,
                 unsigned flags);
void irq_free(irq_t *irq);

//

void irq_enable(irq_t *irq);
void irq_disable(irq_t *irq);

//

bool irq_set_affinity(irq_t *irq, unsigned cpu_id);

// Manually raise an interrupt (emulated)
void irq_raise(irq_t *irq);
