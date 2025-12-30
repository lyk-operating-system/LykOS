#include "fs/ramfs.h"

#include "arch/clock.h"
#include "arch/types.h"
#include "hhdm.h"
#include "log.h"
#include "mm/heap.h"
#include "mm/mm.h"
#include "mm/pm.h"
#include "uapi/errno.h"
#include "utils/list.h"
#include "utils/math.h"
#include "utils/string.h"
#include <stdint.h>

#define INITIAL_PAGE_CAPACITY 1

typedef struct
{
    void *data;
}
ramfs_page_t;

typedef struct ramfs_node ramfs_node_t;

struct ramfs_node
{
    vnode_t vn;

    ramfs_node_t *parent;
    list_t children;
    ramfs_page_t *pages;
    size_t page_count;
    size_t page_capacity;

    list_node_t list_node;
};

// VFS API

static vnode_t *ramfs_get_root(vfs_t *self);

vfs_ops_t ramfs_ops = {
    .get_root = ramfs_get_root
};

static int read  (vnode_t *self, void *buffer, uint64_t count, uint64_t offset, uint64_t *out);
static int write (vnode_t *self, const void *buffer, uint64_t count, uint64_t offset, uint64_t *out);
static int lookup(vnode_t *self, const char *name, vnode_t **out);
static int create(vnode_t *self, const char *name, vnode_type_t t, vnode_t **out);

vnode_ops_t ramfs_node_ops = {
    .read   = read,
    .write  = write,
    .lookup = lookup,
    .create = create
};

// Filesystem Operations

static vnode_t *ramfs_get_root(vfs_t *self)
{
    return (vnode_t *)self->private_data;
}

// Node Operations

static int read(vnode_t *self, void *buffer, uint64_t count, uint64_t offset, uint64_t *out)
{
    ramfs_node_t *node = (ramfs_node_t *)self;

    uint64_t page_start = 0;
    uint64_t copied = 0;

    for (size_t i = 0; i < node->page_count; i++)
    {
        ramfs_page_t *page = &node->pages[i];

        if (offset >= page_start && page_start < offset + count)
        {
            uint64_t page_offset = offset - page_start;
            uint64_t bytes_to_copy = MIN(ARCH_PAGE_GRAN - page_offset, count - copied);
            memcpy((uint8_t *)buffer + copied, (uint8_t *)page->data + page_offset, bytes_to_copy);
            copied += bytes_to_copy;
        }

        page_start += ARCH_PAGE_GRAN;

        if (copied >= count)
            break;
    }

    node->vn.atime = arch_clock_get_unix_time();

    *out = copied;
    return EOK;
}

static int write(vnode_t *self, const void *buffer, uint64_t count, uint64_t offset, uint64_t *out)
{
    ramfs_node_t *node = (ramfs_node_t *)self;
    uint64_t page_start = 0;
    uint64_t copied = 0;
    uint64_t needed_page_count = CEIL(offset + count, ARCH_PAGE_GRAN) / ARCH_PAGE_GRAN;

    if (needed_page_count > node->page_capacity)
    {
        size_t new_capacity = MAX(needed_page_count, node->page_capacity * 2);

        node->pages = heap_realloc(
            node->pages,
            sizeof(ramfs_page_t) * node->page_capacity,
            sizeof(ramfs_page_t) * new_capacity
        );
        node->page_capacity = new_capacity;
    }

    while (node->page_count < needed_page_count)
    {
        node->pages[node->page_count].data = (void *)(pm_alloc(0) + HHDM);
        node->page_count++;
    }

    for (size_t i = 0; i < node->page_count; i++)
    {
        ramfs_page_t *page = &node->pages[i];

        if (offset >= page_start && page_start < offset + count)
        {
            uint64_t page_offset = offset - page_start;
            uint64_t bytes_to_copy = MIN(ARCH_PAGE_GRAN - page_offset, count - copied);

            memcpy((uint8_t *)page->data + page_offset,
                   (uint8_t *)buffer + copied,
                   bytes_to_copy);

            copied += bytes_to_copy;
        }

        page_start += ARCH_PAGE_GRAN;
    }

    if (offset + copied > node->vn.size)
        node->vn.size = offset + copied;

    node->vn.mtime = arch_clock_get_unix_time();

    *out = copied;
    return EOK;
}

static int lookup(vnode_t *self, const char *name, vnode_t **out)
{
    ramfs_node_t *current = (ramfs_node_t *)self;

    if (strcmp(name, ".") == 0)
    {
        *out = self;
        return EOK;
    }
    if (strcmp(name, "..") == 0)
    {
        *out = current->parent ? &current->parent->vn : self;
        return EOK;
    }
    FOREACH(n, current->children)
    {
        ramfs_node_t *child = LIST_GET_CONTAINER(n, ramfs_node_t, list_node);
        if (strcmp(child->vn.name, name) == 0)
        {
            *out = &child->vn;
            return EOK;
        }
    }

    *out = NULL;
    return ENOENT;
}

static int create(vnode_t *self, const char *name, vnode_type_t t, vnode_t **out)
{
    uint64_t now = arch_clock_get_unix_time();

    ramfs_node_t *current = (ramfs_node_t *)self;
    ramfs_node_t *child = heap_alloc(sizeof(ramfs_node_t));
    *child = (ramfs_node_t) {
        .vn = (vnode_t) {
            .name = strdup(name),
            .type = t,
            .perm = 0,
            .ctime = now,
            .mtime = now,
            .atime = now,
            .size = 0,
            .ops  = &ramfs_node_ops,
            .inode = child,
            .ref_count = 1
        },
        .parent = current,
        .children = LIST_INIT,
        .pages = heap_alloc(sizeof(ramfs_page_t) * INITIAL_PAGE_CAPACITY),
        .page_count = 0,
        .page_capacity = INITIAL_PAGE_CAPACITY,
        .list_node = LIST_NODE_INIT,
    };

    list_append(&current->children, &child->list_node);

    *out = &child->vn;
    return EOK;
}

//

vfs_t *ramfs_create()
{
    uint64_t now = arch_clock_get_unix_time();

    ramfs_node_t *ramfs_root = heap_alloc(sizeof(ramfs_node_t));
    *ramfs_root = (ramfs_node_t) {
        .vn = {
            .name = strdup("/"),
            .type = VDIR,
            .ctime = now,
            .mtime = now,
            .atime = now,
            .size = 0,
            .ops  = &ramfs_node_ops,
            .inode = &ramfs_root,
            .ref_count = 1
        },
        .parent = ramfs_root,
        .children = LIST_INIT,
        .pages = heap_alloc(sizeof(ramfs_page_t) * INITIAL_PAGE_CAPACITY),
        .page_count = 0,
        .page_capacity = INITIAL_PAGE_CAPACITY,
        .list_node = LIST_NODE_INIT,
    };

    vfs_t *ramfs_vfs = heap_alloc(sizeof(vfs_t));
    *ramfs_vfs = (vfs_t) {
        .name = strdup("ramfs"),
        .vfs_ops = &ramfs_ops,
        .covered_vn = NULL,
        .flags = 0,
        .block_size = ARCH_PAGE_GRAN,
        .private_data = ramfs_root
    };

    log(LOG_INFO, "RAMFS: new filesystem created.");
    return ramfs_vfs;
}
