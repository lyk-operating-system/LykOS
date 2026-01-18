#pragma once

#include "uapi/errno.h"
#include "utils/list.h"
#include <stdint.h>
#include <stdatomic.h>

#define VFS_MAX_NAME_LEN 128
#define VNODE_MAX_NAME_LEN 128
#define PATH_MAX_NAME_LEN 256

typedef struct vfs vfs_t;
typedef struct vfs_ops vfs_ops_t;
typedef struct vnode vnode_t;
typedef struct vnode_ops vnode_ops_t;

// VFS Structure and Operations

struct vfs
{
    char *name;
    vfs_ops_t *vfs_ops;
    vnode_t *covered_vn;
    int flags;
    size_t block_size;
    void *private_data;

    list_node_t list_node;
};

struct vfs_ops
{
    vnode_t *(*get_root)(vfs_t *vfs);
};

// VNode structure and operations

typedef enum
{
    VNON,
    VREG,
    VDIR,
    VBLK,
    VCHR,
    VLNK,
    VSOCK,
    VBAD
}
vnode_type_t;

struct vnode
{
    char *name;
    vnode_type_t type;
    uint32_t perm;
    uint64_t ctime;
    uint64_t mtime;
    uint64_t atime;
    uint64_t size;

    vnode_ops_t *ops;
    void *inode;

    atomic_int refcount;
};

typedef struct
{
    char name[VFS_MAX_NAME_LEN + 1];
    vnode_type_t type;
    // TODO: Add other fields
}
vfs_dirent_t;

int vfs_destroy(vnode_t *vn);

/**
 * @brief Increment vnode reference count.
 *
 * Acquires an additional reference to the vnode.
 * This does not imply ownership transfer; the caller must already
 * hold a valid reference.
 *
 * @param vn Pointer to the vnode.
 */
static inline void vnode_ref(vnode_t *vn)
{
    atomic_fetch_add_explicit(&vn->refcount, 1, memory_order_relaxed);
}

/**
 * @brief Decrement vnode reference count and deallocate if it reaches zero.
 *
 * Releases a reference to the vnode. When the reference count drops to zero,
 * the vnode deallocates itself.
 *
 * @param vn Pointer to the vnode.
 */
static inline bool vnode_unref(vnode_t *vn)
{
    if (atomic_fetch_sub_explicit(&vn->refcount, 1, memory_order_acq_rel) == 1)
    {
        vfs_destroy(vn);
        return true;
    }
    return false;
}

struct vnode_ops
{
    int (*open)   (vnode_t *vn, int flags, void *cred);
    int (*close)  (vnode_t *vp, int flags, void *cred);
    int (*destroy)(vnode_t *vn, int flags);
    int (*read)   (vnode_t *vn, void *buffer, uint64_t offset, uint64_t count, uint64_t *out_bytes_read);
    int (*write)  (vnode_t *vn, const void *buffer, uint64_t offset, uint64_t count, uint64_t *out_bytes_written);
    int (*lookup) (vnode_t *vn, const char *name, vnode_t **out_vn);
    int (*create) (vnode_t *vn, const char *name, vnode_type_t type, vnode_t **out_vn);
    int (*remove) (vnode_t *vn, const char *name);
    int (*mkdir)  (vnode_t *vn, const char *name, vnode_t **out_vn);
    int (*rmdir)  (vnode_t *vn, const char *name);
    int (*readdir)(vnode_t *vn, vfs_dirent_t **out_entries, size_t *out_count);
    int (*ioctl)  (vnode_t *vn, uint64_t cmd, void *arg);
};

/*
 * Veneer layer.
*/

[[nodiscard]] int vfs_read(vnode_t *vn, void *buffer, uint64_t offset, uint64_t count, uint64_t *out_bytes_read);
[[nodiscard]] int vfs_write(vnode_t *vn, void *buffer, uint64_t offset, uint64_t count, uint64_t *out_bytes_written);
[[nodiscard]] int vfs_open(vnode_t *vn, int flags, void *cred);
[[nodiscard]] int vfs_close(vnode_t *vn, int flags, void *cred);
[[nodiscard]] int vfs_lookup(const char *path, vnode_t **out_vn);
[[nodiscard]] int vfs_create(const char *path, vnode_type_t type, vnode_t **out_vn);
[[nodiscard]] int vfs_remove(const char *path);
[[nodiscard]] int vfs_ioctl(vnode_t *vn, uint64_t cmd, void *arg);

void vfs_init();