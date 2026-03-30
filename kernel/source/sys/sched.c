#include "sys/sched.h"

#include "assert.h"
#include "arch/lcpu.h"
#include "arch/timer.h"
#include "sync/spinlock.h"
#include "sys/smp.h"
#include "sys/thread.h"
#include "utils/list.h"

#define MLFQ_LEVELS 16

static list_t ready_queues[MLFQ_LEVELS] = { [0 ... MLFQ_LEVELS - 1] = LIST_INIT };
static const uint64_t timeslices[MLFQ_LEVELS] = {
    1000,    // 1 ms
    2000,
    4000,
    8000,
    16000,
    32000,
    64000,
    128000,  // cap at 128ms
    100000,
    100000,
    100000,
    100000,
    100000,
    100000,
    100000,
    100000
};
static spinlock_t slock = SPINLOCK_INIT;

// Private API

static thread_t *pick_next_thread()
{
    for (size_t lvl = 0; lvl < MLFQ_LEVELS; lvl++)
        FOREACH(n, ready_queues[lvl])
        {
            thread_t *t = LIST_GET_CONTAINER(n, thread_t, sched_thread_list_node);
            if (t->sleep_until < arch_timer_get_uptime_ns())
            {
                list_remove(&ready_queues[lvl], n);
                t->status = THREAD_STATE_RUNNING;
                return t;
            }
        }

    return sched_get_curr_thread()->assigned_cpu->idle_thread;
}

// This function will be called from the assembly function `__thread_context_switch`.
void sched_drop(thread_t *t)
{
    ASSERT(t);
    ASSERT(t->assigned_cpu);
    ASSERT(t->assigned_cpu->idle_thread);

    if (t == t->assigned_cpu->idle_thread)
        return;

    if (t->status != THREAD_STATE_READY)
        return;

    t->assigned_cpu = NULL;

    spinlock_acquire(&slock);
    list_append(&ready_queues[t->priority], &t->sched_thread_list_node);
    spinlock_release(&slock);
}

// Public API

thread_t *sched_get_curr_thread()
{
    return (thread_t *)arch_lcpu_thread_reg_read();
}

uint32_t sched_get_curr_cpuid()
{
    return sched_get_curr_thread()->assigned_cpu->id;
}

void sched_enqueue(thread_t *t)
{
    spinlock_acquire(&slock);
    t->last_ran = 0;
    t->sleep_until = 0;
    t->status = THREAD_STATE_READY;
    list_append(&ready_queues[0], &t->sched_thread_list_node);
    spinlock_release(&slock);
}

void sched_preempt()
{
    arch_timer_stop();

    spinlock_acquire(&slock);
    thread_t *old = sched_get_curr_thread();
    old->last_ran = arch_timer_get_uptime_ns();
    if (old->priority < MLFQ_LEVELS - 1)
        old->priority++;
    old->status = THREAD_STATE_READY;
    thread_t *new = pick_next_thread();
    new->assigned_cpu = old->assigned_cpu;
    spinlock_release(&slock);

    arch_timer_oneshot(timeslices[new->priority]);

    vm_addrspace_load(new->owner->as);
    arch_thread_context_switch(&old->context, &new->context); // this calls sched_drop()
}

void sched_yield(thread_status_t status)
{
    arch_timer_stop();

    spinlock_acquire(&slock);
    thread_t *old = sched_get_curr_thread();
    old->last_ran = arch_timer_get_uptime_ns();
    old->status = status;
    thread_t *new = pick_next_thread();
    new->assigned_cpu = old->assigned_cpu;
    spinlock_release(&slock);

    arch_timer_oneshot(timeslices[new->priority]);

    vm_addrspace_load(new->owner->as);
    arch_thread_context_switch(&old->context, &new->context); // this calls sched_drop()
}

void sched_init_cpu()
{
    arch_timer_stop();
    arch_timer_set_handler_per_cpu(sched_preempt);
}
