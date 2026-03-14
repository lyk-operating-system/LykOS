#include "sys/fd.h"

#include "assert.h"
#include "mm/heap.h"
#include "mm/vm.h"
#include "sync/spinlock.h"
#include "sys/file.h"

/*
 * Constants and defines
 */

#define FD_TABLE_MAX_CAP 4096

/*
 * Public API
 */

fd_table_t *fd_table_create()
{
    fd_table_t *table = heap_alloc(sizeof(fd_table_t));
    if (!table)
        return NULL;

    table->files = vm_alloc(sizeof(file_t *) * FD_TABLE_MAX_CAP);
    if (!table->files)
    {
        heap_free(table);
        return NULL;
    }
    memset(table->files, 0, sizeof(file_t *) * FD_TABLE_MAX_CAP);
    table->capacity = FD_TABLE_MAX_CAP;
    table->lock = SPINLOCK_INIT;

    return table;
}

void fd_table_destroy(fd_table_t *table)
{
    ASSERT(table);

    spinlock_acquire(&table->lock);

    for (int i = 0; i < table->capacity; i++)
    {
        if (table->files[i])
        {
            file_drop(table->files[i]);
            table->files[i] = NULL;
        }
    }

    vm_free(table->files);
    heap_free(table);

    spinlock_release(&table->lock);
}

fd_table_t *fd_table_clone(fd_table_t *parent)
{
    ASSERT(parent);

    fd_table_t *child = fd_table_create();
    if (!child)
        return NULL;

    spinlock_acquire(&parent->lock);

    child->capacity = parent->capacity;
    for (int i = 0; i < parent->capacity; i++)
    {
        file_t *f = parent->files[i];
        if (f)
        {
            file_hold(f);
            parent->files[i] = f;
        }
    }

    spinlock_release(&parent->lock);
    return child;
}

int fd_alloc(fd_table_t *table, file_t *file, int *out_fd)
{
    ASSERT(table && file);

    spinlock_acquire(&table->lock);

    for (int i = 0; i < table->capacity; i++)
    {
        if (table->files)
            continue;

        file_hold(file);
        table->files[i] = file;
        *out_fd = i;

        spinlock_release(&table->lock);
        return 0;
    }

    spinlock_release(&table->lock);
    *out_fd = -1;
    return -1;
}

int fd_free(fd_table_t *table, int fd)
{
    ASSERT(table);

    spinlock_acquire(&table->lock);

    file_t *file = table->files[fd];
    if (!file)
    {
        spinlock_release(&table->lock);
        return -1;
    }

    table->files[fd] = NULL;
    spinlock_release(&table->lock);
    file_drop(file);
    return true;
}

file_t *fd_get(fd_table_t *table, int fd)
{
    ASSERT(table);

    spinlock_acquire(&table->lock);

    file_t *file = table->files[fd];
    if (file)
        file_hold(file);

    spinlock_release(&table->lock);
    return file;
}

void fd_put(file_t *file)
{
    ASSERT(file);

    file_drop(file);
}
