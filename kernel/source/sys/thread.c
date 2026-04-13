#include "arch/thread.h"

#include "assert.h"
#include "fs/vfs.h"
#include "mm/heap.h"
#include "mm/mm.h"
#include "sys/thread.h"
#include "uapi/errno.h"
#include "utils/ref.h"

static uint32_t next_tid = 0;
static spinlock_t slock = SPINLOCK_INIT;

static inline uint32_t new_tid()
{
    spinlock_acquire(&slock);
    uint32_t ret = next_tid++;
    spinlock_release(&slock);

    return ret;
}

int thread_create_kernel(vm_addrspace_t *as, uintptr_t entry, size_t stack_size,
                         thread_t **out_thread)
{
    ASSERT(as && out_thread);

    int err = EOK;

    thread_t *thread = heap_alloc(sizeof(thread_t));
    if (!thread)
        return ENOMEM;
    thread->tid = new_tid();
    thread->owner = NULL;
    thread->priority = 0;
    thread->status = THREAD_STATE_NEW;
    thread->last_ran = 0;
    thread->sleep_until = 0;
    thread->assigned_cpu = NULL;
    thread->proc_thread_list_node = LIST_NODE_INIT;
    thread->sched_thread_list_node = LIST_NODE_INIT;
    thread->slock = SPINLOCK_INIT;
    thread->refcount = REF_INIT;
    const char *argv[] = { "test", NULL };
    const char *envp[] = { NULL };
    err = arch_thread_context_init(&thread->context, as, false, entry,
                                   stack_size, argv, envp);
    if (err != EOK)
    {
        heap_free(thread);
        *out_thread = NULL;
        return err;
    }

    *out_thread = thread;
    return EOK;
}

int thread_create_user(vm_addrspace_t *as, uintptr_t entry, size_t stack_size,
                       const char *const argv[], const char *const envp[],
                       thread_t **out_thread)
{
    ASSERT(as && out_thread);

    int err = EOK;

    thread_t *thread = heap_alloc(sizeof(thread_t));
    if (!thread)
        return ENOMEM;
    thread->tid = new_tid();
    thread->owner = NULL; // To be set by caller.
    thread->priority = 0;
    thread->status = THREAD_STATE_NEW;
    thread->last_ran = 0;
    thread->sleep_until = 0;
    thread->assigned_cpu = NULL;
    thread->proc_thread_list_node = LIST_NODE_INIT;
    thread->sched_thread_list_node = LIST_NODE_INIT;
    thread->slock = SPINLOCK_INIT;
    thread->refcount = REF_INIT;
    err = arch_thread_context_init(&thread->context, as, true, entry,
                                   stack_size, argv, envp);
    if (err != EOK)
    {
        heap_free(thread);
        *out_thread = NULL;
        return err;
    }

    *out_thread = thread;
    return EOK;
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
    if (!arch_thread_context_fork(&new_thread->context, &thread->context))
        goto fail;
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
    new_thread->refcount = REF_INIT;

    return new_thread;

fail:
    heap_free(new_thread);

    return NULL;
}
