#pragma once

#include "fs/vfs.h"

typedef struct
{
    vfs_t *vfs;          // Mounted filesystem
    vnode_t *mountpoint; // Directory vnode where this fs is mounted
    unsigned flags;
}
vfsmount_t;

int mount(const char *path, vfs_t *vfs, unsigned flags);
int unmount(const char *path);

vfsmount_t *find_mount(const char *path, const char **rest);

/*
 * Initialization
 */

void mount_init(vfs_t *vfs);
