#include "fs/vfs.h"

#include "fs/ramfs.h"
#include "log.h"
#include "mm/heap.h"
#include "mm/mm.h"
#include "uapi/errno.h"
#include "utils/list.h"
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

// Utils

static trie_node_t *find_child(trie_node_t *parent, const char *comp, size_t length)
{
    for (size_t i = 0; i < parent->children_cnt; i++)
        if (strncmp(parent->children[i]->comp, comp, length) == 0)
            return parent->children[i];
    return NULL;
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

/*
 * Veneer layer.
 */

int vfs_read(vnode_t *vn, void *buffer, uint64_t offset, uint64_t count, uint64_t *out_bytes_read)
{
    if (!vn || !buffer)
        return EINVAL;

    return vn->ops->read(vn, buffer, offset, count, out_bytes_read);
}

int vfs_write(vnode_t *vn, void *buffer, uint64_t offset, uint64_t count, uint64_t *out_bytes_written)
{
    if (!vn || !buffer)
        return EINVAL;

    return vn->ops->write(vn, buffer, offset, count, out_bytes_written);
}

int vfs_lookup(const char *path, vnode_t **out)
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

    *out = curr;
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

int vfs_ioctl(vnode_t *vn, uint64_t cmd, void *arg)
{
    if (!vn->ops->ioctl)
        return ENOTTY;
    return vn->ops->ioctl(vn, cmd, arg);
}

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
