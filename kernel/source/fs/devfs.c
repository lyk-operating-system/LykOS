#include "fs/devfs.h"

#include "fs/ramfs.h"
#include "log.h"
#include "mm/heap.h"
#include "utils/string.h"
#include "uapi/errno.h"

bool devfs_register_device(const char *path, vnode_type_t type,
                           vnode_ops_t *ops, void *priv_data)
{
    vnode_t *vn;

    int err = vfs_create(path, type, &vn);
    if (err != EOK)
        return false;

    vn->ops = ops;
    vn->inode = priv_data;
    return true;
}

void devfs_unregister_device(const char *path)
{
    if (vfs_remove(path) != EOK)
        log(LOG_ERROR, "Could not unreigster devfs device!");
}

/*
 * Initialization
 */

void devfs_init()
{
    vfs_t *devfs = ramfs_create();
    heap_free(devfs->name);
    devfs->name = strdup("devfs");

    log(LOG_INFO, "DevFS initialized and mounted at /devfs");
}
