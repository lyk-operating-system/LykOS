#include "proc/proc.h"

#include "assert.h"
#include "mm/heap.h"
#include "mm/vm.h"
#include "proc/fd.h"
#include "proc/thread.h"
#include "utils/list.h"
#include "utils/string.h"

static uint32_t next_pid = 0;
static list_t proc_list = LIST_INIT;
static spinlock_t slock = SPINLOCK_INIT;

static inline uint32_t new_pid()
{
    spinlock_acquire(&slock);
    uint32_t ret = next_pid++;
    spinlock_release(&slock);

    return ret;
}

proc_t *proc_create(const char *name, bool user)
{
    ASSERT(name);

    proc_t *proc = heap_alloc(sizeof(proc_t));
    if (!proc)
        return NULL;
    proc->pid = new_pid();
    proc->ppid = 0; // To be set by caller.
    proc->name = strdup(name);
    if (!proc->name)
        goto cleanup;
    proc->user = user;
    proc->status = PROC_STATE_NEW;
    proc->as = user ? vm_addrspace_create() : vm_kernel_as;
    if (user && !proc->as)
        goto cleanup;
    proc->threads = LIST_INIT;
    proc->fd_table = heap_alloc(sizeof(fd_table_t));
    if (!proc->fd_table)
        goto cleanup;
    fd_table_init(proc->fd_table);
    proc->cwd = NULL;
    proc->proc_list_node = LIST_NODE_INIT;
    proc->slock = SPINLOCK_INIT;
    proc->ref_count = 1;

    spinlock_acquire(&slock);
    list_append(&proc_list, &proc->proc_list_node);
    spinlock_release(&slock);

    return proc;

cleanup:
    if (proc->name) heap_free(proc->name);
    if (proc->fd_table) heap_free(proc->fd_table);
    if (user && proc->as) vm_addrspace_destroy(proc->as);
    heap_free(proc);
    return NULL;
}

void proc_destroy(proc_t *proc)
{
    ASSERT(proc);

    // Remove from process pool
    spinlock_acquire(&slock);
    list_remove(&proc_list, &proc->proc_list_node);
    spinlock_release(&slock);

    // Free threads
    while (!list_is_empty(&proc->threads))
    {
        list_node_t *n = list_pop_head(&proc->threads);
        thread_t *t = LIST_GET_CONTAINER(n, thread_t, proc_thread_list_node);
        thread_destroy(t);
    }

    // Free resources
    heap_free((void *)proc->name);
    vm_addrspace_destroy(proc->as);
    fd_table_destroy(proc->fd_table);
    heap_free((void *)proc->cwd);

    heap_free(proc);
}

proc_t *proc_fork(proc_t *proc)
{
    ASSERT(proc);

    // Create new process structure
    proc_t *new_proc = heap_alloc(sizeof(proc_t));
    if (!new_proc)
        return NULL;
    new_proc->pid = new_pid();
    new_proc->name = strdup(proc->name);
    if (!new_proc->name)
        goto cleanup;
    new_proc->ppid = proc->pid;
    new_proc->user= proc->user;
    new_proc->status = PROC_STATE_NEW;
    new_proc->as = vm_addrspace_clone(proc->as);
    if (!new_proc->as)
        goto cleanup;
    new_proc->threads = LIST_INIT;
    new_proc->fd_table = fd_table_clone(proc->fd_table);
    if (!new_proc->fd_table)
        goto cleanup;
    new_proc->cwd = strdup(proc->cwd);
    if (!new_proc->cwd)
        goto cleanup;
    new_proc->proc_list_node = LIST_NODE_INIT;
    new_proc->slock = SPINLOCK_INIT;
    new_proc->ref_count = 1;

    // Duplicate threads
    FOREACH(n, proc->threads)
    {
        thread_t *thread = LIST_GET_CONTAINER(n, thread_t, proc_thread_list_node);
        thread_t *new_thread = thread_duplicate(thread);
        if (!new_thread)
            goto cleanup;
        new_thread->owner = new_proc;
        list_append(&new_proc->threads, &new_thread->proc_thread_list_node);
    }

    return new_proc;

cleanup:
    if (new_proc->name) heap_free(new_proc->name);
    if (new_proc->as) vm_addrspace_destroy(new_proc->as);
    if (new_proc->fd_table) fd_table_destroy(new_proc->fd_table);
    if (new_proc->cwd) heap_free(new_proc->cwd);

    while (!list_is_empty(&new_proc->threads))
    {
        list_node_t *n = list_pop_head(&new_proc->threads);
        thread_t *t = LIST_GET_CONTAINER(n, thread_t, proc_thread_list_node);
        thread_destroy(t);
    }

    heap_free(new_proc);

    return NULL;
}
