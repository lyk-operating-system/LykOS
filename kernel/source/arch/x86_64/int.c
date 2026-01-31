#include "arch/x86_64/devices/lapic.h"
#include "panic.h"

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

static void (*arch_timer_handler)();

void arch_timer_set_handler_per_cpu(void (*handler)())
{
    arch_timer_handler = handler;
}

void arch_int_handler(cpu_state_t *cpu_state)
{
    if (cpu_state->int_no < 32) // Exceptions
    {
        panic("CPU EXCEPTION: %llx %#llx", cpu_state->int_no, cpu_state->err_code);
    }
    else // IRQs
    {
        uint8_t irq = cpu_state->int_no - 32;

        switch (irq)
        {
            case 0: // PIT
                break;
            case 1: // PS/2

                break;
            case 2: // LAPIC Timer
                arch_timer_handler();
                break;
            default:
                panic("Unhandled IRQ %d", irq);
                break;
        }
    }

    x86_64_lapic_send_eoi();
}
