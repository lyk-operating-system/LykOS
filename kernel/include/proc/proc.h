#pragma once

#include "mm/vm.h"
#include "utils/list.h"
#include "proc/fd.h"
#include "sync/spinlock.h"

typedef enum
{
    PROC_STATE_NEW,
    PROC_STATE_TERMINATED,
}
proc_status_t;

typedef struct proc
{
    size_t pid;
    const char *name;
    bool user;

    proc_status_t status;
    vm_addrspace_t *as;
    list_t threads;

    fd_table_t *fd_table;
    const char *cwd;

    list_node_t proc_list_node;
    spinlock_t slock;
    size_t ref_count;
}
proc_t;

// Create and destroy

proc_t *proc_create(const char *name, bool is_kernel);
void proc_destroy(proc_t *proc);
