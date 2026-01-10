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
    irq_handler_fn handler;
    void *context;
    irq_handle_t handle;
}
irq_desc_t;

static irq_desc_t irqs[256]; // we shouldnt share these slots between all CPUs
static spinlock_t slock = SPINLOCK_INIT;

bool arch_irq_alloc(
    irq_handler_fn handler,
    void *context,
    uint32_t target_cpuid,
    irq_handle_t *out_irq_handle
)
{
    spinlock_acquire(&slock);

    for (size_t i = 64; i < 256; i++)
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

        spinlock_release(&slock);
        *out_irq_handle = desc->handle;
        return true;
    }

    spinlock_release(&slock);
    return false;
}

void arch_irq_free(irq_handle_t handle)
{
    spinlock_acquire(&slock);

    irqs[handle.vector].handler = NULL;

    spinlock_release(&slock);
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
    if (cpu_state->int_no < 32) // Exceptions
    {
        panic("CPU EXCEPTION: %llx %#llx", cpu_state->int_no, cpu_state->err_code);
    }
    else if (cpu_state->int_no < 64) // IRQs - reserved for timer, ps/2 keyboard etc.
    {
        switch (cpu_state->int_no)
        {
            case 40:

                break;
            default:
                break;
        }
    }
    else // IRQs - reserved for peripherals
    {
        spinlock_acquire(&slock);
        irq_desc_t desc = irqs[cpu_state->int_no];
        spinlock_release(&slock);

        if (!desc.handler)
            panic("Unused IRQ was dispatched!");

        desc.handler(desc.handle, desc.context);
    }

    x86_64_lapic_send_eoi();
}
