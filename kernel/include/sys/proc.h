#pragma once

#include "mm/vm.h"
#include "utils/list.h"
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
    uint32_t ppid;
    char *name;
    bool user;

    proc_status_t status;
    vm_addrspace_t *as;
    list_t threads;

    fd_table_t *fd_table;
    char *cwd;

    list_node_t proc_list_node;
    spinlock_t slock;
    size_t ref_count;
}
proc_t;

/*
 * Create, destroy, and fork
 */

proc_t *proc_create(const char *name, const char *cwd, bool user);

void proc_destroy(proc_t *proc);

proc_t *proc_fork(proc_t *proc, thread_t *calling_thread);
