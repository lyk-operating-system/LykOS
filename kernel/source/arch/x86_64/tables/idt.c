// API
#include "arch/x86_64/tables/idt.h"
#include "arch/irq.h"
//
#include "arch/lcpu.h"
#include "arch/x86_64/tables/gdt.h"
#include "log.h"
#include "proc/sched.h"
#include "sync/spinlock.h"
#include <stddef.h>
#include <stdint.h>

typedef struct
{
    uint16_t isr_low;
    uint16_t kernel_cs;
    uint8_t  ist;
    uint8_t  flags;
    uint16_t isr_mid;
    uint32_t isr_high;
    uint32_t _rsv;
}
__attribute__((packed))
idt_entry_t;

typedef struct
{
    uint16_t limit;
    uint64_t base;
}
__attribute__((packed))
idtr_t;

__attribute__((aligned(0x10))) static idt_entry_t idt[256];
extern uintptr_t __int_stub_table[256];

void x86_64_idt_init()
{
    for (int i = 0; i < 256; i++)
    {
        uint64_t isr = (uint64_t)__int_stub_table[i];

        idt[i] = (idt_entry_t) {
            .isr_low = isr & 0xFFFF,
            .kernel_cs = GDT_SELECTOR_CODE64_RING0,
            .ist = 0,
            .flags = 0x8E,
            .isr_mid = (isr >> 16) & 0xFFFF,
            .isr_high = (isr >> 32) & 0xFFFFFFFF,
            ._rsv = 0
        };
    }

    log(LOG_INFO, "IDT generated.");
}

void x86_64_idt_init_cpu()
{
    idtr_t idtr = (idtr_t) {
        .limit = sizeof(idt) - 1,
        .base = (uint64_t)&idt
    };
    asm volatile("lidt %0" : : "m"(idtr));

    arch_lcpu_int_unmask();

    log(LOG_INFO, "IDT loaded");
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

#define MAX_CPU_COUNT 32

#define CURR_CPU (sched_get_curr_thread()->assigned_cpu->id)

static struct
{
    bool allocated;
    void (*handler)();
}
irq_handlers[MAX_CPU_COUNT][128];
static spinlock_t slock = SPINLOCK_INIT;

void arch_int_handler(cpu_state_t *cpu_state)
{
    if (cpu_state->int_no < 32)
    {
        log(LOG_INFO, "CPU EXCEPTION: %llx %#llx", cpu_state->int_no, cpu_state->err_code);
        arch_lcpu_halt();
    }
    else
    {
        spinlock_acquire(&slock);

        size_t irq = cpu_state->int_no - 32;
        if (irq_handlers[CURR_CPU][irq].allocated && irq_handlers[CURR_CPU][irq].handler)
            irq_handlers[CURR_CPU][irq].handler();

        spinlock_release(&slock);
    }
}

// irq.h API

bool arch_irq_reserve_local(size_t local_irq)
{
    spinlock_acquire(&slock);

    if (irq_handlers[CURR_CPU][local_irq].allocated)
    {
        spinlock_release(&slock);
        return false;
    }

    irq_handlers[CURR_CPU][local_irq].allocated = true;
    spinlock_release(&slock);
    return true;
}

bool arch_irq_alloc_local(size_t *out)
{
    spinlock_acquire(&slock);

    for (size_t i = 0; i < 256; i++)
        if (!irq_handlers[CURR_CPU][i].allocated)
        {
            *out = i;
            spinlock_release(&slock);
            return true;
        }

    spinlock_release(&slock);
    return false;
}

void arch_irq_free_local(size_t local_irq)
{
    spinlock_acquire(&slock);

    irq_handlers[CURR_CPU][local_irq].allocated = false;

    spinlock_release(&slock);
}

void arch_irq_set_local_handler(size_t local_irq, uintptr_t handler)
{
    spinlock_acquire(&slock);

    irq_handlers[CURR_CPU][local_irq].handler = (void(*)())handler;

    spinlock_release(&slock);
}
