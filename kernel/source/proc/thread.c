#include "arch/thread.h"
#include "assert.h"
#include "proc/thread.h"

#include "mm/heap.h"

static uint32_t next_tid = 0;
static spinlock_t slock = SPINLOCK_INIT;

static inline uint32_t new_tid()
{
    spinlock_acquire(&slock);
    uint32_t ret = next_tid++;
    spinlock_release(&slock);

    return ret;
}

thread_t *thread_create(proc_t *proc, uintptr_t entry)
{
    thread_t *thread = heap_alloc(sizeof(thread_t));
    if (!thread)
        return NULL;
    thread->tid = new_tid();
    thread->owner = proc;
    thread->priority = 0;
    thread->status = THREAD_STATE_NEW;
    thread->last_ran = 0;
    thread->sleep_until = 0;
    thread->assigned_cpu = NULL;
    thread->proc_thread_list_node = LIST_NODE_INIT;
    thread->sched_thread_list_node = LIST_NODE_INIT;
    thread->slock = SPINLOCK_INIT;
    thread->ref_count = 1;
    arch_thread_context_init(&thread->context, proc->as, proc->user, entry);

    spinlock_acquire(&proc->slock);
    list_append(&proc->threads, &thread->proc_thread_list_node);
    spinlock_release(&proc->slock);

    return thread;
}

void thread_destroy(thread_t *thread)
{
    ASSERT(thread && thread->status == THREAD_STATE_TERMINATED);
}

thread_t *thread_duplicate(thread_t *thread)
{
    ASSERT(thread);

    thread_t *new_thread = heap_alloc(sizeof(thread_t));
    if (!new_thread)
        return NULL;
    if (!arch_thread_context_copy(&new_thread->context, &thread->context))
        goto cleanup;
    new_thread->tid = new_tid();
    new_thread->owner = NULL; // To be set by caller.

    new_thread->priority = 0;
    new_thread->status = THREAD_STATE_NEW;
    new_thread->last_ran = 0;
    new_thread->sleep_until = 0;
    new_thread->assigned_cpu = NULL;

    new_thread->proc_thread_list_node = LIST_NODE_INIT;
    new_thread->sched_thread_list_node = LIST_NODE_INIT;
    new_thread->slock = SPINLOCK_INIT;
    new_thread->ref_count = 1;

    return new_thread;

cleanup:
    heap_free(new_thread);

    return NULL;
}
