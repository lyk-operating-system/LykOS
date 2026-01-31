#pragma once

typedef struct irq irq_t;

typedef enum
{
    PORT_IRQ_EDGE_RISING,
    PORT_IRQ_EDGE_FALLING,
    PORT_IRQ_LEVEL_HIGH,
    PORT_IRQ_LEVEL_LOW,
}
irq_trigger_t;

typedef bool (*irq_handler_t)(irq_t *irq, void *data);

irq_t *irq_alloc(irq_trigger_t trigger, irq_handler_t handler,
                 unsigned int flags);
void irq_free(irq_t *irq);

void irq_enable(irq_t *irq);
void irq_disable(irq_t *irq);

int irq_set_affinity(irq_t *irq, unsigned cpu_id);

// Manually raise an interrupt (emulated)
void irq_raise(irq_t *irq);
