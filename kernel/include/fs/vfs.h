#pragma once

#include "sync/spinlock.h"
#include "utils/list.h"
#include "utils/ref.h"
#include "utils/xarray.h"
#include <stdint.h>

typedef struct vm_addrspace vm_addrspace_t;

typedef struct vnode vnode_t;
typedef struct vnode_ops vnode_ops_t;
typedef struct vfs_dirent vfs_dirent_t;
typedef struct vfs_ops vfs_ops_t;

#define VFS_MAX_NAME_LEN 128
#define VNODE_MAX_NAME_LEN 128
#define PATH_MAX_NAME_LEN 256

/*
 * VFS Structure and Operations
 */

typedef struct
{
    char *name;
    vfs_ops_t *vfs_ops;
    vnode_t *covered_vn;
    int flags;
    size_t block_size;
    void *private_data;

    list_node_t list_node;
}
vfs_t;

struct vfs_ops
{
    vnode_t *(*get_root)(vfs_t *vfs);
};

/*
 * VNode structure and operations
 */

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
    // Metadata
    char *name;
    vnode_type_t type;
    uint32_t perm;
    uint64_t ctime;
    uint64_t mtime;
    uint64_t atime;
    uint64_t size;

    // Page cache
    xarray_t pages;

    // FS-specific ops and data
    vnode_ops_t *ops;
    void *inode;

    // Misc
    ref_t refcount;
    spinlock_t slock;
};

struct vfs_dirent
{
    char name[VNODE_MAX_NAME_LEN + 1];
    vnode_type_t type;
    // TODO: Add other fields
};

struct vnode_ops
{
    // Read/Write
    int (*read) (vnode_t *vn, void *buffer, uint64_t offset, uint64_t count,
                 uint64_t *out_bytes_read);
    int (*write)(vnode_t *vn, const void *buffer, uint64_t offset, uint64_t count,
                 uint64_t *out_bytes_written);
    // Directory
    int (*lookup) (vnode_t *vn, const char *name, vnode_t **out_vn);
    int (*create) (vnode_t *vn, const char *name, vnode_type_t type, vnode_t **out_vn);
    int (*remove) (vnode_t *vn, const char *name);
    int (*mkdir)  (vnode_t *vn, const char *name, vnode_t **out_vn);
    int (*rmdir)  (vnode_t *vn, const char *name);
    int (*readdir)(vnode_t *vn, vfs_dirent_t **out_entries, size_t *out_count);
    // Misc
    int (*ioctl)(vnode_t *vn, uint64_t cmd, void *args);
    int (*mmap) (vnode_t *vn, vm_addrspace_t *as, uintptr_t vaddr, size_t length,
                 int prot, int flags, uint64_t offset);
};

void vnode_hold(vnode_t *vn);
void vnode_drop(vnode_t *vn);

/*
 * Veneer layer.
*/

// Read/Write
[[nodiscard]] int vfs_read(vnode_t *vn, void *buffer, uint64_t offset, uint64_t count, uint64_t *out_bytes_read);
[[nodiscard]] int vfs_write(vnode_t *vn, void *buffer, uint64_t offset, uint64_t count, uint64_t *out_bytes_written);
// Directory
[[nodiscard]] int vfs_lookup(const char *path, vnode_t **out_vn);
[[nodiscard]] int vfs_create(const char *path, vnode_type_t type, vnode_t **out_vn);
[[nodiscard]] int vfs_remove(const char *path);
// Misc
[[nodiscard]] int vfs_ioctl(vnode_t *vn, uint64_t cmd, void *args);
[[nodiscard]] int vfs_mmap(vnode_t *vn, vm_addrspace_t *as, uintptr_t vaddr,
                           size_t length, int prot, int flags, uint64_t offset);

/*
 * Initialization
 */

void vfs_init();
