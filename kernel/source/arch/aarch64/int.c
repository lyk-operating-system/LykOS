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
    bool used;
    void (*handler)(uint32_t irq, void *context);
    void *context;
    uint32_t target_cpuid;
}
irq_desc_t;

#define MAX_IRQ 1020

static irq_desc_t irqs[MAX_IRQ];
static spinlock_t slock = SPINLOCK_INIT;

bool arch_irq_alloc(
    void (*handler)(uint32_t irq, void *context),
    void *context,
    uint32_t target_cpuid,
    uint32_t *out_irq
)
{
    spinlock_acquire(&slock);

    for (size_t i = 0; i < MAX_IRQ; i++)
    {
        irq_desc_t *desc = &irqs[i];

        if (desc->used)
            continue;

        *desc = (irq_desc_t) {
            .used = true,
            .handler = handler,
            .context = context,
            .target_cpuid = target_cpuid
        };

        aarch64_gic->disable_int(i);
        // aarch64_gic->clear_pending(irq);
        aarch64_gic->set_target(i, target_cpuid);

        // We leave it up to the caller to also enable the IRQ.

        spinlock_release(&slock);
        *out_irq = i;
        return true;
    }

    spinlock_release(&slock);
    return false;
}

void arch_irq_free(uint32_t irq)
{
    spinlock_acquire(&slock);

    irqs[irq].used = false;
    // It doesn't hurt to disable the IRQ.
    aarch64_gic->disable_int(irq);
    // aarch64_gic->clear_pending(irq);

    spinlock_release(&slock);
}

void arch_irq_enable(uint32_t irq)
{
    aarch64_gic->enable_int(irq);
}

void arch_irq_disable(uint32_t irq)
{
    aarch64_gic->disable_int(irq);
}

void arch_irq_dispatch(uint32_t irq)
{
    spinlock_acquire(&slock);
    irq_desc_t desc = irqs[irq];
    spinlock_release(&slock);

    if (!desc.used)
        panic("Unused IRQ was dispatched!");

    desc.handler(irq, desc.context);
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
                arch_irq_dispatch(intid);

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
