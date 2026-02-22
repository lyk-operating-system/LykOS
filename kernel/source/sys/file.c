#include "sys/file.h"

#include "assert.h"
#include "fs/vfs.h"
#include "mm/heap.h"

file_t *file_create_vnode(vnode_t *vn, int flags)
{
    ASSERT(vn);

    file_t *file = heap_alloc(sizeof(file_t));
    if (!file)
        return NULL;
    *file = (file_t) {
        .type = FILE_TYPE_VNODE,
        .ops = NULL, // TODO: this
        .data = vn,
        .flags = flags,
        .offset = 0,
        .refcount = REF_INIT,
        .slock = SPINLOCK_INIT
    };

    return file;
}

void file_hold(file_t *file)
{
    ref_inc(&file->refcount);
}

void file_drop(file_t *file)
{
    if (ref_dec(&file->refcount))
        heap_free(file);
}
