#include "sys/proc.h"

#include "assert.h"
#include "log.h"
#include "mm/heap.h"
#include "mm/mm.h"
#include "mm/vm.h"
#include "sys/elf.h"
#include "sys/fd.h"
#include "sys/sched.h"
#include "sys/thread.h"
#include "uapi/errno.h"
#include "utils/container_of.h"
#include "utils/list.h"
#include "utils/ref.h"
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

int proc_create_kernel(const char *name, proc_t **out_proc)
{
    ASSERT(name);

    int err = EOK;

    proc_t *proc = heap_alloc(sizeof(proc_t));
    if (!proc)
    {
        err = ENOMEM;
        goto fail;
    }
    memset(proc, 0, sizeof(proc_t));
    proc->pid = new_pid();
    proc->parent = NULL;
    proc->name = strdup(name);
    if (!proc->name)
    {
        err = ENOMEM;
        goto fail;
    }
    proc->user = false;
    proc->status = PROC_STATE_NEW;
    proc->as = vm_kernel_as;
    proc->threads = LIST_INIT;
    proc->fd_table = NULL;
    proc->cwd = NULL;
    proc->proc_list_node = LIST_NODE_INIT;
    proc->slock = SPINLOCK_INIT;
    proc->refcount = REF_INIT;

    spinlock_acquire(&slock);
    list_append(&proc_list, &proc->proc_list_node);
    spinlock_release(&slock);

    *out_proc = proc;
    return EOK;

fail:
    log(LOG_ERROR, "Failed to create kernel process!");

    if (!proc)
    {
        *out_proc = NULL;
        return ENOMEM;
    }
    if (proc->cwd) heap_free(proc->cwd);
    heap_free(proc);

    *out_proc = NULL;
    return err;
}

int proc_create_user(proc_t *parent, const char *path, const char *const argv[],
                     const char *const envp[], proc_t **out_proc)
{
    ASSERT(path && argv && envp && out_proc);

    int err = EOK;

    proc_t *proc = heap_alloc(sizeof(proc_t));
    if (!proc)
    {
        err = ENOMEM;
        goto fail;
    }
    memset(proc, 0, sizeof(proc_t));
    proc->pid = new_pid();
    proc->parent = parent;
    proc->name = NULL;
    proc->user = true;
    proc->status = PROC_STATE_NEW;
    proc->as = vm_addrspace_create();
    if (!proc->as)
    {
        err = ENOMEM;
        goto fail;
    }
    proc->threads = LIST_INIT;
    proc->fd_table = fd_table_create();
    if (!proc->fd_table)
    {
        err = ENOMEM;
        goto fail;
    }
    proc->cwd = parent ? strdup(parent->cwd) : strdup("/");
    if (!proc->cwd)
    {
        err = ENOMEM;
        goto fail;
    }
    proc->proc_list_node = LIST_NODE_INIT;
    proc->slock = SPINLOCK_INIT;
    proc->refcount = REF_INIT;

    void *entry = NULL;
    char *interpreter = NULL;
    err = elf_load(proc->as, path, &entry, &interpreter);
    if (err != EOK)
        goto fail;

    thread_t *initial_thread = NULL;
    err = thread_create_user(proc->as, (uintptr_t)entry, 2 * MIB, argv, envp,
                             &initial_thread);
    if (err != EOK)
        goto fail;
    initial_thread->owner = proc;
    list_append(&proc->threads, &initial_thread->proc_thread_list_node);

    spinlock_acquire(&slock);
    list_append(&proc_list, &proc->proc_list_node);
    spinlock_release(&slock);

    sched_enqueue(initial_thread);

    *out_proc = proc;
    return EOK;

fail:
    log(LOG_ERROR, "Failed to create user process!");

    if (!proc)
    {
        *out_proc = NULL;
        return ENOMEM;
    }
    if (proc->as) vm_addrspace_destroy(proc->as);
    if (proc->fd_table) fd_table_destroy(proc->fd_table);
    if (proc->cwd) heap_free(proc->cwd);
    heap_free(proc);

    *out_proc = NULL;
    return err;
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
        thread_t *t = container_of(n, thread_t, proc_thread_list_node);
        thread_destroy(t);
    }

    // Free resources
    if (proc->name)
        heap_free((void *)proc->name);
    if (proc->user)
    {
        vm_addrspace_destroy(proc->as);
        fd_table_destroy(proc->fd_table);
        heap_free((void *)proc->cwd);
    }

    heap_free(proc);
}

int proc_execve(proc_t *proc, const char *path,
                const char *const argv[],
                const char *const envp[])
{
    ASSERT(proc && path && argv && envp);

    int err = EOK;

    vm_addrspace_t *new_as = vm_addrspace_create();
    if (!new_as)
        return ENOMEM;

    void *entry = NULL;
    char *interpreter = NULL;
    err = elf_load(new_as, path, &entry, &interpreter);
    if (err != EOK)
    {
        vm_addrspace_destroy(new_as);
        return err;
    }

    thread_t *initial_thread;
    err = thread_create_user(new_as, (uintptr_t)entry, 8192, argv, envp, &initial_thread);
    if (err != EOK)
    {
        vm_addrspace_destroy(new_as);
        return err;
    }
    initial_thread->owner = proc;

    while (!list_is_empty(&proc->threads))
    {
        list_node_t *n = list_pop_head(&proc->threads);
        thread_t *t = container_of(n, thread_t, proc_thread_list_node);
        thread_destroy(t);
    }
    vm_addrspace_destroy(proc->as);

    proc->as = new_as;
    list_append(&proc->threads, &initial_thread->proc_thread_list_node);
    sched_enqueue(initial_thread);

    return EOK;
}

proc_t *proc_fork(proc_t *proc, thread_t *calling_thread)
{
    ASSERT(proc);

    // Create new process structure
    proc_t *new_proc = heap_alloc(sizeof(proc_t));
    if (!new_proc)
        goto fail;
    new_proc->pid = new_pid();
    new_proc->name = strdup(proc->name);
    if (!new_proc->name)
        goto fail;
    new_proc->parent = proc->parent;
    new_proc->user= proc->user;
    new_proc->status = PROC_STATE_NEW;
    new_proc->as = vm_addrspace_clone(proc->as);
    if (!new_proc->as)
        goto fail;
    new_proc->threads = LIST_INIT;
    new_proc->fd_table = fd_table_clone(proc->fd_table);
    if (!new_proc->fd_table)
        goto fail;
    new_proc->cwd = strdup(proc->cwd);
    if (!new_proc->cwd)
        goto fail;
    new_proc->proc_list_node = LIST_NODE_INIT;
    new_proc->slock = SPINLOCK_INIT;
    new_proc->refcount = REF_INIT;

    // Duplicate the calling thread
    thread_t *new_thread = thread_duplicate(calling_thread);
    if (!new_thread)
        goto fail;
    new_thread->owner = new_proc;
    list_append(&new_proc->threads, &new_thread->proc_thread_list_node);

    spinlock_acquire(&slock);
    list_append(&proc_list, &new_proc->proc_list_node);
    spinlock_release(&slock);

    return new_proc;

fail:
    log(LOG_ERROR, "Failed to fork process!");

    if (!new_proc) return NULL;
    if (new_proc->name) heap_free(new_proc->name);
    if (new_proc->as) vm_addrspace_destroy(new_proc->as);
    if (new_proc->fd_table) fd_table_destroy(new_proc->fd_table);
    if (new_proc->cwd) heap_free(new_proc->cwd);

    heap_free(new_proc);

    return NULL;
}
