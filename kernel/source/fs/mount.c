#include "fs/mount.h"

#include "assert.h"
#include "crypto/hash.h"
#include "fs/path.h"
#include "mm/heap.h"
#include "mm/mm.h"
#include "uapi/errno.h"
#include "utils/string.h"

typedef struct trie_node
{
    size_t hash;
    size_t len;

    size_t children_cnt;
    struct trie_node *children[16];

    vfsmount_t *vfsmount;

    const char *comp;
}
trie_node_t;

/*
 * Global data
 */

static trie_node_t trie_root;

/*
 * Helpers
 */

static trie_node_t *find_child(trie_node_t *parent, size_t hash, const char *comp, size_t len)
{
    for (size_t i = 0; i < parent->children_cnt; i++)
    {
        trie_node_t *child = parent->children[i];

        if (child->hash == hash
        &&  child->len == len
        &&  strncmp(child->comp, comp, len) == 0)
            return child;
    }

    return NULL;
}

/*
 * API
 */

int mount(const char *path, vfs_t *vfs, unsigned flags)
{
    trie_node_t *current = &trie_root;
    char component[PATH_MAX];
    size_t len;

    while (*path)
    {
        path = path_next_component(path, component, &len);
        if (len == 0)
            break;

        size_t hash = djb2_len(component, len);
        trie_node_t *next = find_child(current, hash, component, len);

        if (!next)
        {
            // TODO: make trie count variable
            ASSERT(current->children_cnt < 16);

            next = heap_alloc(sizeof(trie_node_t));
            if (!next)
                return ENOMEM;
            *next = (trie_node_t) {
                .hash = hash,
                .len = len
            };

            // Duplicate component string for the trie's lifetime
            char *comp_name = heap_alloc(len + 1);
            memcpy(comp_name, component, len);
            comp_name[len] = '\0';
            next->comp = comp_name;

            current->children[current->children_cnt++] = next;
        }
        current = next;
    }

    // Check if something is already mounted.
    if (current->vfsmount)
        return EBUSY;

    vfsmount_t *mnt = heap_alloc(sizeof(vfsmount_t));
    if (!mnt)
        return ENOMEM;
    *mnt = (vfsmount_t) {
        .vfs = vfs,
        .mountpoint = NULL,
        .flags = flags,
    };
    current->vfsmount = mnt;

    return EOK;
}

vfsmount_t *find_mount(const char *path, const char **rest)
{
    trie_node_t *current = &trie_root;
    vfsmount_t *last_match = trie_root.vfsmount;

    const char *last_rest = (*path == '/') ? path + 1 : path;

    while (*path)
    {
        while (*path == '/')
            path++;

        const char *start = path;
        while (*path && *path != '/')
            path++;

        size_t len = path - start;
        if (len == 0) break;

        size_t hash = djb2_len(start, len);
        trie_node_t *next = find_child(current, hash, start, len);

        if (!next)
            break;

        current = next;
        if (current->vfsmount)
        {
            last_match = current->vfsmount;
            last_rest = (*path == '/') ? path + 1 : path;
        }
    }

    if (rest)
        *rest = last_rest;

    return last_match;
}

/*
 * Initialization
 */

void mount_init(vfs_t *vfs)
{
    ASSERT(vfs);

    vfsmount_t *root_mnt = heap_alloc(sizeof(vfsmount_t));
    if (!root_mnt)
        panic("Failed to allocate root vfsmount!");
    *root_mnt = (vfsmount_t) {
        .vfs = vfs,
        .mountpoint = NULL, // Root has no parent directory.
        .flags = 0,
    };

    trie_root = (trie_node_t) {
        .vfsmount = root_mnt,
    };
}
