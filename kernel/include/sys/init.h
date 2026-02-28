#pragma once

#include "fs/vfs.h"
#include "sys/proc.h"

proc_t *init_load(vnode_t *file);
