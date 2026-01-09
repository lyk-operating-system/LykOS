// API
#include "arch/irq.h"
//
#include "arch/x86_64/devices/lapic.h"
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

static irq_desc_t irqs[32][64]; // Maximum of 32 CPUs with 64 allocatable IRQs each.
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

    spinlock_release(&slock);
}

void arch_irq_enable(uint32_t irq)
{

}

void arch_irq_disable(uint32_t irq)
{

}

void arch_irq_dispatch(uint32_t irq)
{
    spinlock_acquire(&slock);
    irq_desc_t desc = irqs[irq / 64][irq % 64];
    spinlock_release(&slock);

    if (!desc.used)
        panic("Unused IRQ was dispatched!");

    desc.handler(irq, desc.context);
}

// Interrupt handling

typedef struct
{
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rdx, rcx, rbx, rax;
    uint64_t int_no;
    uint64_t err_code, rip, cs, rflags, rsp, ss;
}
__attribute__((packed))
cpu_state_t;

void arch_int_handler(cpu_state_t *cpu_state)
{
    if (cpu_state->int_no < 32)
    {
        log(LOG_INFO, "CPU EXCEPTION: %llx %#llx", cpu_state->int_no, cpu_state->err_code);
        arch_lcpu_halt();
    }
    else
    {
        arch_irq_dispatch(cpu_state->int_no);
    }

    x86_64_lapic_send_eoi();
}
