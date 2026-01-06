#include "proc/fd.h"

#include "mm/heap.h"
#include "panic.h"
#include "sync/spinlock.h"

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

bool fd_alloc(fd_table_t *table, vnode_t *vnode, int *fd)
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

            *fd = (int)i;
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
    // TODO: reallocate FD table properly!
    // panic("Cannot reallocate FD table for now!");

    fd_entry_t *new_fds = heap_realloc(
        table->fds,
        old_capacity * sizeof(fd_entry_t),
        new_capacity * sizeof(fd_entry_t)
    );

    if (!new_fds) return false;

    for (size_t i = old_capacity; i < new_capacity; i++)
    {
        new_fds[i].vnode = NULL;
        new_fds[i].offset = 0;
        fd_init_ref(&new_fds[i]);
    }

    spinlock_acquire(&table->lock);

    if (table->capacity != old_capacity)
    {
        spinlock_release(&table->lock);
        return fd_alloc(table, vnode, fd);
    }

    table->fds = new_fds;
    table->capacity = new_capacity;

    fd_entry_t *entry = &table->fds[old_capacity];
    entry->vnode = vnode;
    entry->offset = 0;
    fd_init_ref(entry);
    vnode_ref(vnode);

    *fd = (int)old_capacity;
    spinlock_release(&table->lock);
    return true;
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

fd_entry_t *fd_get(fd_table_t *table, int fd)
{
    spinlock_acquire(&table->lock);

    if (fd >= 0 && (size_t)fd < table->capacity && table->fds[fd].vnode != NULL)
    {
        fd_entry_t *entry = &table->fds[fd];
        fd_ref(entry);
        spinlock_release(&table->lock);
        return entry;
    }

    spinlock_release(&table->lock);
    return NULL;
}

void fd_put(fd_entry_t *entry)
{
    fd_unref(entry);
}
