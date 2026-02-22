#pragma once

#include "sync/spinlock.h"

/*
 * Forward declarations
 */

typedef struct file file_t;

/*
 * File descriptor table
 */

typedef struct fd_table
{
    file_t **files;
    int capacity; // int is fine here because fds are ints

    spinlock_t lock;
}
fd_table_t;

/*
 * Public API
 */

fd_table_t *fd_table_create();
void        fd_table_destroy(fd_table_t *table);
fd_table_t *fd_table_clone(fd_table_t *table);

int fd_alloc(fd_table_t *table, file_t *file, int *out_fd);
int fd_free(fd_table_t *table, int fd);

file_t *fd_get(fd_table_t *table, int fd);
void    fd_put(file_t *file);
