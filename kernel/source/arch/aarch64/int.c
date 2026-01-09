// API.h
#include "arch/irq.h"
//
#include "arch/aarch64/devices/gic.h"
#include "arch/lcpu.h"
#include "log.h"
#include "panic.h"
#include "sync/spinlock.h"

// irq.h API

typedef struct
{
    irq_handler_fn handler;
    void *context;
    irq_handle_t handle;
}
irq_desc_t;

#define MAX_IRQ 1020

static irq_desc_t irqs[MAX_IRQ];
static spinlock_t slock = SPINLOCK_INIT;

bool arch_irq_alloc(
    irq_handler_fn handler,
    void *context,
    uint32_t target_cpuid,
    irq_handle_t *out_irq_handle
)
{
    spinlock_acquire(&slock);

    for (size_t i = aarch64_gic->min_global_irq; i < aarch64_gic->max_global_irq; i++)
    {
        irq_desc_t *desc = &irqs[i];

        if (desc->handler)
            continue;

        *desc = (irq_desc_t) {
            .handler = handler,
            .context = context,
            .handle = (irq_handle_t) {
                .vector = i,
                .target_cpuid = target_cpuid
            }
        };

        aarch64_gic->disable_int(i);
        // aarch64_gic->clear_pending(irq);
        aarch64_gic->set_target(i, target_cpuid);
        aarch64_gic->enable_int(i);

        spinlock_release(&slock);
        *out_irq_handle = (irq_handle_t) {
            .vector = i,
            .target_cpuid = target_cpuid
        };
        return true;
    }

    spinlock_release(&slock);
    return false;
}

void arch_irq_free(irq_handle_t handle)
{
    spinlock_acquire(&slock);

    aarch64_gic->disable_int(handle.vector);
    irqs[handle.vector].handler = NULL;
    // aarch64_gic->clear_pending(irq);

    spinlock_release(&slock);
}

// Interrupt handling

typedef struct
{
    uint64_t x[31];
}
__attribute__((packed))
cpu_state_t;

void aarch64_int_handler(
    const uint64_t source,
    cpu_state_t const *cpu_state,
    const uint64_t esr, const uint64_t elr,
    const uint64_t spsr, const uint64_t far
)
{
    (void)cpu_state;

    switch (source)
    {
        // Synchronous
        case 0:
        case 4:
        {
            log(
                LOG_FATAL,
                "SYNC exception ESR=%lx ELR=%lx FAR=%lx SPSR=%lx",
                esr, elr, far, spsr
            );
            panic("sync");
        }

        // IRQ
        case 1: // current EL
        case 5: // lower EL
        {
            uint32_t iar = aarch64_gic->ack_int();
            uint32_t intid = iar & 0x3ff;

            if (intid < 1020)
            {
                spinlock_acquire(&slock);
                irq_desc_t desc = irqs[intid];
                spinlock_release(&slock);

                if (!desc.handler)
                    panic("Unused IRQ was dispatched!");

                desc.handler(desc.handle, desc.context);
            }

            aarch64_gic->end_of_int(iar);
            return;
        }

        // FIQ
        case 2:
        case 6:
            panic("FIQ");

        // SError
        case 3:
        case 7:
            panic("SError");

        default:
            panic("EVT ");
    }
}

// Initialization

extern void __arch64_evt_load();

void aarch64_int_init_cpu()
{
    arch_lcpu_int_mask();
    __arch64_evt_load();
    arch_lcpu_int_unmask();

    log(LOG_DEBUG, "Initialized interrupts.");
}
