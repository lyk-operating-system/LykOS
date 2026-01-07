#pragma once

#include <stddef.h>
#include "fs/vfs.h"
#include "sync/spinlock.h"
#include <stdatomic.h>

#define MAX_FD_COUNT 16

typedef struct
{
    bool read;
    bool write;
    bool exec;
    bool search;
}
fd_acc_mode_t;

typedef struct fd_entry
{
    vnode_t *vnode;
    size_t offset;
    fd_acc_mode_t acc_mode;
    atomic_int refcount;
}
fd_entry_t;

typedef struct fd_table
{
    fd_entry_t *fds;
    size_t capacity;
    spinlock_t lock;
}
fd_table_t;

void fd_table_init(fd_table_t *table);
void fd_table_destroy(fd_table_t *table);

fd_table_t *fd_table_clone(fd_table_t *table);

bool fd_alloc(fd_table_t *table, vnode_t *vnode, fd_acc_mode_t acc_mode, int *out_fd);
bool fd_free(fd_table_t *table, int fd);

fd_entry_t fd_get(fd_table_t *table, int fd);
void fd_put(fd_table_t *table, int fd);
