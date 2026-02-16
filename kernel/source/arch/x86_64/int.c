#include "arch/irq.h"

#include "arch/x86_64/devices/ioapic.h"
#include "arch/x86_64/devices/lapic.h"
#include "mm/heap.h"
#include "panic.h"
#include "proc/sched.h"
#include "sync/spinlock.h"
#include <stddef.h>

/*
 * Per-CPU vector managament
 */

#define MAX_CPUS 256
#define MAX_VEC_CPU 128

typedef struct
{
    irq_handler_t handlers[MAX_VEC_CPU];
    spinlock_t slock;
}
cpu_vector_group_t;
cpu_vector_group_t cpu_vec_grps[MAX_CPUS];

static int alloc_vector(unsigned cpu_id, irq_handler_t handler)
{
    cpu_vector_group_t *vg = &cpu_vec_grps[cpu_id];
    spinlock_acquire(&vg->slock);

    for (size_t i = 48; i < MAX_VEC_CPU; i++)
        if (vg->handlers[i] == NULL)
        {
            vg->handlers[i] = handler;
            spinlock_release(&vg->slock);
            return i;
        }

    spinlock_release(&vg->slock);
    return -1;
}

static void free_vector(unsigned cpu_id, int vector)
{
    cpu_vector_group_t *vg = &cpu_vec_grps[cpu_id];
    spinlock_acquire(&vg->slock);
    vg->handlers[vector] = NULL;
    spinlock_release(&vg->slock);
}

/*
 * arch/irq.h API
 */

struct irq
{
    bool legacy;
    int gsi;
    int vector;
    int cpu_id;
    irq_trigger_t trigger;
    irq_handler_t handler;
};

irq_t *irq_claim_legacy(unsigned gsi,
                        irq_trigger_t trigger,
                        irq_handler_t handler,
                        unsigned flags)
{
    irq_t *i = heap_alloc(sizeof(irq_t));
    *i = (irq_t) {
        .legacy = true,
        .gsi = gsi,
        .vector = -1,
        .cpu_id = -1,
        .trigger = trigger,
        .handler = handler
    };

    return i;
}

irq_t *irq_alloc(irq_trigger_t trigger, irq_handler_t handler,
                 unsigned flags)
{
    irq_t *i = heap_alloc(sizeof(irq_t));
    *i = (irq_t) {
        .legacy = false,
        .gsi = x86_64_ioapic_allocate_gsi(),
        .vector = -1,
        .cpu_id = -1,
        .trigger = trigger,
        .handler = handler
    };
    if (i->gsi < 0)
    {
        heap_free(i);
        return NULL;
    }

    return i;
}

void irq_free(irq_t *irq)
{
    if (!irq->legacy)
        x86_64_ioapic_free_gsi(irq->gsi);
    if (irq->vector >= 0)
        free_vector(irq->cpu_id, irq->vector);
    heap_free(irq);
}

void irq_enable(irq_t *irq)
{

}

void irq_disable(irq_t *irq)
{

}

bool irq_set_affinity(irq_t *irq, unsigned cpu_id)
{
    int old_cpu_id = irq->cpu_id;
    int old_vector = irq->vector;
    irq->vector = alloc_vector(cpu_id, irq->handler);
    if (irq->vector < 0)
        return false;
    if (old_cpu_id >= 0 && old_vector >= 0)
        free_vector(old_cpu_id, old_vector);
    irq->cpu_id = cpu_id;

    if (irq->legacy)
        x86_64_ioapic_map_legacy_irq(irq->gsi, cpu_id, false, false, irq->vector);
    else
        x86_64_ioapic_map_gsi(irq->gsi, cpu_id, false, false, irq->vector);
    return true;
}

void irq_raise(irq_t *irq)
{

}

/*
 * Interrupt handling
 */

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

static_assert(LAPIC_TIMER_VECTOR == 32);

void arch_int_handler(cpu_state_t *cpu_state)
{
    if (cpu_state->int_no < 32) // Exceptions
    {
        if (((cpu_state->cs & 0x3) == 3)
        && cpu_state->int_no == 14) // PF from userspace
        {
            uint64_t cr2;
            __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));

            vm_page_fault(sched_get_curr_thread()->owner->as, cr2);
        }
        else
            panic("CPU EXCEPTION: %llx %#llx", cpu_state->int_no, cpu_state->err_code);
    }
    else // IRQs
    {
        if (cpu_state->int_no == LAPIC_TIMER_VECTOR)
        {
            arch_timer_handler();
        }
        else
        {
            irq_handler_t handler;

            unsigned cpu_id = sched_get_curr_thread()->assigned_cpu->id;
            cpu_vector_group_t *vg = &cpu_vec_grps[cpu_id];
            spinlock_acquire(&vg->slock);
            handler = vg->handlers[cpu_state->int_no];
            spinlock_release(&vg->slock);

            if (!handler || !handler(NULL, NULL))
                panic("Unhandled vector %d", cpu_state->int_no);
        }
    }

    x86_64_lapic_send_eoi();
}
