#pragma once

#include "fs/vfs.h"

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

vfs_t *ramfs_create();
