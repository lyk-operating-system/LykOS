#pragma once

#include "fs/vfs.h"

bool devfs_register_device(const char *path, vnode_type_t type,
                           vnode_ops_t *ops, void *priv_data);
void devfs_unregister_device(const char *path);

void devfs_init();
