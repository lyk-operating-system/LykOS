#include "proc/proc.h"

#include "mm/heap.h"
#include "mm/vm.h"
#include "proc/fd.h"
#include "proc/thread.h"
#include "utils/list.h"
#include "utils/string.h"

static uint64_t next_pid = 0;
static list_t proc_list = LIST_INIT;
static spinlock_t slock = SPINLOCK_INIT;

proc_t *proc_create(const char *name, bool user)
{
    proc_t *proc = heap_alloc(sizeof(proc_t));

    *proc = (proc_t) {
        .pid = next_pid,
        .name = strdup(name),
        .status = PROC_STATE_NEW,
        .user = user,
        .as = user ? vm_addrspace_create() : vm_kernel_as,
        .threads = LIST_INIT,
        .proc_list_node = LIST_NODE_INIT,
        .slock = SPINLOCK_INIT,
        .ref_count = 1,
        .fd_table = proc->fd_table
    };

    fd_table_init(&proc->fd_table);

    spinlock_acquire(&slock);
    next_pid++;
    list_append(&proc_list, &proc->proc_list_node);
    spinlock_release(&slock);

    return proc;
}

void proc_kill(proc_t *proc)
{
    fd_table_destroy(&proc->fd_table);
    vm_addrspace_destroy(proc->as);
    heap_free(proc);
}

void proc_destroy(proc_t *proc)
{
    if (!proc) return;

    if (proc->as)
    {
        vm_addrspace_destroy(proc->as);
        proc->as = NULL;
    }

    fd_table_destroy(&proc->fd_table);

    while (!list_is_empty(&proc->threads))
    {
        list_node_t *node = list_pop_head(&proc->threads);

        /* TO-DO:
        - get corresponding thread
        - destroy thread (add func in thread.h?)
        */
    }

    /* TO-DO:
    - implement global process table/list
    - remove process from there
    */

    spinlock_release(&proc->slock);
    heap_free(proc);
}
