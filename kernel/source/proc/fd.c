#include "proc/fd.h"

#include "fs/vfs.h"
#include "mm/heap.h"
#include "panic.h"
#include "sync/spinlock.h"
#include <stdatomic.h>

// FD lifetime

static inline void fd_init_ref(fd_entry_t *entry)
{
    atomic_init(&entry->refcount, 1);
}

static inline void fd_ref(fd_entry_t *entry)
{
    atomic_fetch_add_explicit(&entry->refcount, 1, memory_order_relaxed);
}

static inline void fd_unref(fd_entry_t *entry)
{
    if (atomic_fetch_sub_explicit(&entry->refcount, 1, memory_order_acq_rel) == 1)
    {
        if (entry->vnode)
            vnode_unref(entry->vnode);
        entry->vnode = NULL;
        entry->offset = 0;
    }
}

//

void fd_table_init(fd_table_t *table)
{
    table->fds = heap_alloc(sizeof(fd_entry_t) * MAX_FD_COUNT);
    table->capacity = MAX_FD_COUNT;
    table->lock = SPINLOCK_INIT;

    for (size_t i = 0; i < table->capacity; i++)
    {
        table->fds[i].vnode = NULL;
        table->fds[i].offset = 0;
    }
}

void fd_table_destroy(fd_table_t *table)
{
    spinlock_acquire(&table->lock);

    for (size_t i = 0; i < table->capacity; i++)
    {
        fd_entry_t *entry = &table->fds[i];
        if (entry->vnode != NULL)
            vnode_unref(entry->vnode);
    }
    heap_free(table->fds);

    spinlock_release(&table->lock);
}

bool fd_alloc(fd_table_t *table, vnode_t *vnode, fd_acc_mode_t acc_mode, int *out_fd)
{
    spinlock_acquire(&table->lock);

    for (size_t i = 0; i < table->capacity; i++)
    {
        if (table->fds[i].vnode == NULL)
        {
            fd_entry_t *entry = &table->fds[i];

            entry->vnode = vnode;
            vnode_ref(vnode);
            entry->offset = 0;
            fd_init_ref(entry);
            entry->acc_mode = acc_mode;

            *out_fd = (int)i;
            spinlock_release(&table->lock);
            return true;
        }
    }

    if (table->capacity >= MAX_FD_COUNT)
    {
        spinlock_release(&table->lock);
        return false;
    }

    size_t old_capacity = table->capacity;
    size_t new_capacity = old_capacity * 2;
    if (new_capacity > MAX_FD_COUNT)
        new_capacity = MAX_FD_COUNT;

    spinlock_release(&table->lock);

    fd_entry_t *new_fds = heap_realloc(
        table->fds,
        old_capacity * sizeof(fd_entry_t),
        new_capacity * sizeof(fd_entry_t)
    );

    if (!new_fds) return false;

    for (size_t i = old_capacity; i < new_capacity; i++)
        new_fds[i] = (fd_entry_t) {
            .vnode = NULL,
            .offset = 0,
            .acc_mode = {false, false, false, false},
            .refcount = 0
        };

    spinlock_acquire(&table->lock);

    if (table->capacity != old_capacity)
    {
        spinlock_release(&table->lock);
        return fd_alloc(table, vnode, acc_mode, out_fd);
    }

    table->fds = new_fds;
    table->capacity = new_capacity;

    fd_entry_t *entry = &table->fds[old_capacity];
    entry->vnode = vnode;
    vnode_ref(vnode);
    entry->offset = 0;
    fd_init_ref(entry);
    entry->acc_mode = acc_mode;

    *out_fd = (int)old_capacity;
    spinlock_release(&table->lock);
    return true;
}

fd_table_t *fd_table_clone(fd_table_t *parent)
{
    fd_table_t *child = heap_alloc(sizeof(fd_table_t));
    if (!child) return NULL;

    fd_table_init(child);
    spinlock_acquire(&parent->lock);

    if (child->capacity < parent->capacity)
    {
        child->fds = heap_realloc(child->fds,
            child->capacity * sizeof(fd_entry_t),
            parent->capacity * sizeof(fd_entry_t));

        if (!child->fds)
        {
            spinlock_release(&parent->lock);
            heap_free(child);
            return NULL;
        }
        child->capacity = parent->capacity;
    }

    for (size_t i = 0; i < parent->capacity; i++)
    {
        child->fds[i] = parent->fds[i];
        if (child->fds[i].vnode)
        {
            vnode_ref(child->fds[i].vnode);
            fd_init_ref(&child->fds[i]);
        }
    }

    spinlock_release(&parent->lock);
    return child;
}

bool fd_free(fd_table_t *table, int fd)
{
    spinlock_acquire(&table->lock);

    if (fd >= 0 && (size_t)fd < table->capacity && table->fds[fd].vnode != NULL)
    {
        fd_unref(&table->fds[fd]);
        spinlock_release(&table->lock);
        return true;
    }

    spinlock_release(&table->lock);
    return false;
}

fd_entry_t fd_get(fd_table_t *table, int fd)
{
    spinlock_acquire(&table->lock);

    if (fd >= 0 && (size_t)fd < table->capacity && table->fds[fd].vnode != NULL)
    {
        fd_entry_t *entry = &table->fds[fd];
        fd_ref(entry);
        spinlock_release(&table->lock);
        return *entry;
    }

    spinlock_release(&table->lock);
    return (fd_entry_t) {-1, NULL, 0, (fd_acc_mode_t) {false, false, false, false}, 0};
}

void fd_put(fd_table_t *table, int fd)
{
    spinlock_acquire(&table->lock);

    fd_entry_t *entry = &table->fds[fd];
    fd_unref(entry);

    spinlock_release(&table->lock);
}
