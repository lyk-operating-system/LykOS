#include "fs/vfs.h"

#include "arch/types.h"
#include "assert.h"
#include "fs/ramfs.h"
#include "hhdm.h"
#include "log.h"
#include "mm/mm.h"
#include "mm/pm.h"
#include "uapi/errno.h"
#include "utils/list.h"
#include "utils/math.h"
#include "utils/string.h"

typedef struct trie_node trie_node_t;

struct trie_node
{
    char *comp;
    vfs_t *vfs;
    vnode_t *mount_vn;

    trie_node_t *children[16];
    size_t children_cnt;
};

static list_t vfs_list = LIST_INIT;
static trie_node_t trie_root;

// Utils and mountpoint

static trie_node_t *find_child(trie_node_t *parent, const char *comp, size_t length)
{
    for (size_t i = 0; i < parent->children_cnt; i++)
        if (strncmp(parent->children[i]->comp, comp, length) == 0)
            return parent->children[i];
    return NULL;
}

static char *vfs_get_mountpoint(const char *path, vnode_t **out)
{
    trie_node_t *current = &trie_root;

    while (*path)
    {
        while(*path == '/')
            path++;
        char *slash = strchr(path, '/');
        size_t length;
        if (slash)
            length = slash - path;
        else
            length = UINT64_MAX;

        trie_node_t *child = find_child(current, path, length);
        if (child)
            current = child;
        else
            break;

        path += strlen(child->comp);
    }

    *out = current->vfs->vfs_ops->get_root(current->vfs);
    return (char *)path;
}

static const char *next_component(const char *path, size_t *out_len)
{
    while (*path == '/')
        path++;

    if (!*path)
        return NULL;

    const char *start = path;
    const char *end = start;
    while (*end && *end != '/')
        end++;

    *out_len = end - start;
    return start;
}

/*
 * Veneer layer.
 */

static int get_page(vnode_t *vn, uint64_t pg_idx, bool read, page_t **out)
{
    page_t *page = xa_get(&vn->pages, pg_idx);
    if (page)
    {
        *out = page;
        return EOK;
    }

    page = pm_alloc(0);
    if (!page)
        return ENOMEM;

    if (read)
    {
        uint64_t read_bytes;
        int err = vn->ops->read(
            vn,
            (void *)(page->addr + HHDM),
            pg_idx * ARCH_PAGE_GRAN,
            ARCH_PAGE_GRAN,
            &read_bytes
        );
        if (err != EOK)
        {
            pm_free(page);
            return err;
        }
    }

    xa_insert(&vn->pages, pg_idx, page);
    *out = page;
    return EOK;
}

int vfs_read(vnode_t *vn, void *buffer, uint64_t offset, uint64_t count,
             uint64_t *out_bytes_read)
{
    ASSERT (vn && buffer && out_bytes_read);

    if (!vn->ops || !vn->ops->read)
        return ENOTSUP;

    uint64_t total_read = 0;
    while (total_read < count)
    {
        uint64_t pos     = offset + total_read;
        uint64_t pg_idx  = pos / ARCH_PAGE_GRAN;
        uint64_t pg_off  = pos % ARCH_PAGE_GRAN;
        uint64_t to_copy = MIN(ARCH_PAGE_GRAN - pg_off, count - total_read);

        page_t *page;
        int err = get_page(vn, pg_idx, true, &page);
        if (err != EOK)
            return err;

        memcpy(
            (uint8_t *)buffer + total_read,
            (uint8_t *)page->addr + HHDM + pg_off,
            to_copy
        );

        total_read += to_copy;
    }

    if (out_bytes_read)
        *out_bytes_read = total_read;
    return EOK;
}

int vfs_write(vnode_t *vn, void *buffer, uint64_t offset, uint64_t count,
              uint64_t *out_bytes_written)
{
    ASSERT (vn && buffer && out_bytes_written);

    if (!vn->ops || !vn->ops->write)
        return ENOTSUP;

    uint64_t total_written = 0;
    while (total_written < count)
    {
        uint64_t pos     = offset + total_written;
        uint64_t pg_idx  = pos / ARCH_PAGE_GRAN;
        uint64_t pg_off  = pos % ARCH_PAGE_GRAN;
        uint64_t to_copy = MIN(ARCH_PAGE_GRAN - pg_off, count - total_written);

        page_t *page;
        int err = get_page(
            vn,
            pg_idx,
            // read-modify-write only if needed
            (pg_off == 0 && to_copy == ARCH_PAGE_GRAN) ? true : false,
            &page
        );
        if (err != EOK)
            return err;

        memcpy(
            (uint8_t *)page->addr + HHDM + pg_off,
            (uint8_t *)buffer + total_written,
            to_copy
        );

        xa_set_mark(&vn->pages, pg_idx, XA_MARK_0); // Mark dirty.
        total_written += to_copy;
    }

    if (offset + total_written > vn->size)
        vn->size = offset + total_written;
    if (out_bytes_written)
        *out_bytes_written = total_written;
    return EOK;
}

// Directory

int vfs_lookup(const char *path, vnode_t **out_vn)
{
    vnode_t *curr;
    path = vfs_get_mountpoint(path, &curr);
    const char *comp;
    size_t comp_len;

    while (curr && (comp = next_component(path, &comp_len)))
    {
        char name_buf[VFS_MAX_NAME_LEN + 1];
        memcpy(name_buf, comp, comp_len);
        name_buf[comp_len] = '\0';

        if (curr->ops->lookup(curr, name_buf, &curr) != EOK)
            return ENOENT;

        path = comp + comp_len;
    }

    *out_vn = curr;
    return EOK;
}

int vfs_create(const char *path, vnode_type_t type, vnode_t **out)
{
    const char *last_slash = strrchr(path, '/');
    char parent_path[PATH_MAX_NAME_LEN];
    char child_name[VFS_MAX_NAME_LEN + 1];

    size_t parent_len = last_slash - path ?: 1;
    strncpy(parent_path, path, parent_len);
    parent_path[parent_len] = '\0';

    strcpy(child_name, last_slash + 1);

    vnode_t *parent;
    int ret = vfs_lookup(parent_path, &parent);
    if (ret != EOK)
    {
        ret = vfs_create(parent_path, VDIR, &parent);
        if (ret != EOK)
            return ret;
    }

    return parent->ops->create(parent, child_name, type, out);
}

int vfs_remove(const char *path)
{
    const char *last_slash = strrchr(path, '/');
    char parent_path[PATH_MAX_NAME_LEN];
    char child_name[VFS_MAX_NAME_LEN + 1];

    size_t parent_len = last_slash - path ?: 1;
    strncpy(parent_path, path, parent_len);
    parent_path[parent_len] = '\0';

    strcpy(child_name, last_slash + 1);

    vnode_t *parent;
    int ret = vfs_lookup(parent_path, &parent);
    if (ret != EOK)
        return ret;

    return parent->ops->remove(parent, child_name);
}

// Misc

int vfs_ioctl(vnode_t *vn, uint64_t cmd, void *args)
{
    ASSERT (vn); // args can be NULL

    if (!vn->ops || !vn->ops->ioctl)
        return ENOTSUP;

    return vn->ops->ioctl(vn, cmd, args);
}

int vfs_mmap(vnode_t *vn, vm_addrspace_t *as, uintptr_t vaddr, size_t length,
             int prot, int flags, uint64_t offset)
{
    ASSERT (vn && as);

    if (!vn->ops || !vn->ops->mmap)
        return ENOTSUP;

    return vn->ops->mmap(vn, as, vaddr, length, prot, flags, offset);
}

/*
 * Initialization
 */

void vfs_init()
{
    vfs_t *ramfs = ramfs_create();

    list_append(&vfs_list, &ramfs->list_node);

    trie_root = (trie_node_t) {
        .comp = strdup("/"),
        .vfs = ramfs,
        .mount_vn = NULL,
        .children = { 0 },
        .children_cnt = 0
    };

    log(LOG_INFO, "VFS initialized.");
}
