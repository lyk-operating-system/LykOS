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
#include "utils/string.h"
#include "utils/xarray.h"
#include <stdint.h>

#define INITIAL_PAGE_CAPACITY 1

typedef struct ramfs_node ramfs_node_t;

struct ramfs_node
{
    vnode_t vn;

    ramfs_node_t *parent;
    list_t children;
    xarray_t pages;
    size_t page_count;

    list_node_t list_node;
};

// VFS API

static vnode_t *ramfs_get_root(vfs_t *self);

vfs_ops_t ramfs_ops = {
    .get_root = ramfs_get_root
};

static int read  (vnode_t *self, void *buf, uint64_t offset, uint64_t count, uint64_t *out);
static int write (vnode_t *self, const void *buf, uint64_t offset, uint64_t count, uint64_t *out);
static int lookup(vnode_t *self, const char *name, vnode_t **out);
static int create(vnode_t *self, const char *name, vnode_type_t t, vnode_t **out);
static int remove(vnode_t *self, const char *name);
static int readdir(vnode_t *self, vfs_dirent_t **out_entries, size_t *out_count);

vnode_ops_t ramfs_node_ops = {
    .read   = read,
    .write  = write,
    .lookup = lookup,
    .create = create,
    .remove = remove,
    .readdir = readdir
};

// Filesystem Operations

static vnode_t *ramfs_get_root(vfs_t *self)
{
    return (vnode_t *)self->private_data;
}

// Node Operations

static int read(vnode_t *self, void *buf, uint64_t offset, uint64_t count, uint64_t *out)
{
    ramfs_node_t *node = (ramfs_node_t *)self;

    uint64_t copied = 0;
    uint8_t *dst = buf;

    while (copied < count)
    {
        uint64_t curr_off = offset + copied;
        size_t page_idx = curr_off / ARCH_PAGE_GRAN;
        size_t page_off = curr_off % ARCH_PAGE_GRAN; // offset within the page

        size_t to_copy = ARCH_PAGE_GRAN - page_off;
        if (to_copy > count - copied)
            to_copy = count - copied;

        void *page = xa_get(&node->pages, page_idx);
        if (!page)
            break; // end of file

        memcpy(dst + copied, (uint8_t *)page + page_off, to_copy);
        copied += to_copy;
    }

    node->vn.atime = arch_clock_get_unix_time();

    *out = copied;
    return EOK;
}

static int write(vnode_t *self, const void *buf, uint64_t offset, uint64_t count, uint64_t *out)
{
    ramfs_node_t *node = (ramfs_node_t *)self;

    uint64_t written = 0;
    const uint8_t *src = buf;

    while (written < count)
    {
        uint64_t curr_off = offset + written;
        size_t page_idx = curr_off / ARCH_PAGE_GRAN;
        size_t page_off = curr_off % ARCH_PAGE_GRAN;

        size_t to_copy = ARCH_PAGE_GRAN - page_off;
        if (to_copy > count - written)
            to_copy = count - written;

        void *page = xa_get(&node->pages, page_idx);
        if (!page)
        {
            page = (void*)(pm_alloc(0)->addr + HHDM);
            xa_insert(&node->pages, page_idx, page);
        }

        memcpy((uint8_t *)page + page_off, src + written, to_copy);
        written += to_copy;
    }

    uint64_t end = offset + written;
    if (end > node->vn.size)
        node->vn.size = end;

    node->vn.mtime = node->vn.ctime = arch_clock_get_unix_time();

    *out = written;
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
            .refcount = 1
        },
        .parent = current,
        .children = LIST_INIT,
        .pages = XARRAY_INIT,
        .page_count = 0,
        .list_node = LIST_NODE_INIT,
    };

    list_append(&current->children, &child->list_node);

    *out = &child->vn;
    return EOK;
}

static int remove(vnode_t *self, const char *name)
{
    ramfs_node_t *current = (ramfs_node_t *)self;
    
    FOREACH(n, current->children)
    {
        ramfs_node_t *child = LIST_GET_CONTAINER(n, ramfs_node_t, list_node);
        if (strcmp(child->vn.name, name) == 0)
        {
            list_remove(&current->children, &child->list_node);
            FOREACH(c, child->children)
            {
                ramfs_node_t *grandchild = LIST_GET_CONTAINER(c, ramfs_node_t, list_node);
                remove(&child->vn, grandchild->vn.name);
            }
            heap_free(child->vn.name);
            heap_free(child);
            return EOK;
        }
    }
    
    return ENOENT;
}

static int readdir(vnode_t *self, vfs_dirent_t **out_entries, size_t *out_count)
{
    if (!out_entries || !out_count)
        return EINVAL;
    if (self->type != VDIR)
        return ENOTDIR;

    ramfs_node_t *dir = (ramfs_node_t *)self;
    size_t entry_count = dir->children.length;

    if (!entry_count)
    {
        *out_entries = NULL;
        *out_count = 0;
        self->atime = arch_clock_get_unix_time();
        return EOK;
    }
    
    vfs_dirent_t *entries = heap_alloc(entry_count * sizeof(vfs_dirent_t));    
    size_t index = 0;
    
    FOREACH(n, dir->children)
    {
        ramfs_node_t *child = LIST_GET_CONTAINER(n, ramfs_node_t, list_node);
        strcpy(entries[index].name, child->vn.name);
        entries[index].type = child->vn.type;
        index++;
    }
    
    self->atime = arch_clock_get_unix_time();
    *out_entries = entries;
    *out_count = entry_count;
    return EOK;
}

static int ioctl(vnode_t *vn, uint64_t cmd, void *arg)
{
    return ENOTSUP;
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
            .refcount = 1
        },
        .parent = ramfs_root,
        .children = LIST_INIT,
        .pages = XARRAY_INIT,
        .page_count = 0,
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
