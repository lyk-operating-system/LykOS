#pragma once

#include "mm/vm.h"
#include "utils/list.h"
#include "utils/ref.h"
#include "sys/fd.h"
#include "sync/spinlock.h"

/*
 * Forward declarations
 */

typedef struct thread thread_t;

//

typedef enum
{
    PROC_STATE_NEW,
    PROC_STATE_TERMINATED,
}
proc_status_t;

typedef struct proc
{
    uint32_t pid;
    struct proc *parent;
    char *name;
    bool user;

    proc_status_t status;
    vm_addrspace_t *as;
    list_t threads;

    fd_table_t *fd_table;
    char *cwd;

    list_node_t proc_list_node;
    ref_t refcount;
    spinlock_t slock;
}
proc_t;

/*
 * Create, destroy, and fork
 */

int proc_create_kernel(const char *name, proc_t **out_proc);

int proc_create_user(proc_t *parent, const char *path, const char *const argv[],
                     const char *const envp[], proc_t **out_proc);

void proc_destroy(proc_t *proc);

proc_t *proc_fork(proc_t *proc, thread_t *calling_thread);
