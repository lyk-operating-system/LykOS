#include "sys/file.h"

#include "assert.h"
#include "fs/vfs.h"
#include "mm/heap.h"

extern const file_ops_t file_vnode_ops;
extern const file_ops_t file_socket_ops;
extern const file_ops_t file_pipe_vnode;
extern const file_ops_t file_fifo_vnode;
extern const file_ops_t file_msgq_vnode;
extern const file_ops_t file_eventq_ops;

file_t *file_alloc(file_type_t type, const file_ops_t *ops, void *backend,
                   int flags)
{
    ASSERT(ops);

    file_t *file = heap_alloc(sizeof(file_t));
    if (!file)
        return NULL;
    *file = (file_t) {
        .type = type,
        .ops = ops,
        .backend = backend,
        .flags = flags,
        .offset = 0,
        .refcount = REF_INIT,
        .slock = SPINLOCK_INIT
    };

    return file;
}

file_t *file_create_vnode(vnode_t *vn, int flags)
{
    ASSERT(vn);

    vnode_hold(vn);

    file_t *f = file_alloc(
        FILE_TYPE_VNODE,
        &file_vnode_ops,
        vn,
        flags
    );
    if (!f)
        vnode_drop(vn);

    return f;
}

file_t *file_create_socket([[maybe_unused]] socket_t *so,
                           [[maybe_unused]] int flags)
{
    ASSERT(so);

    file_t *f = file_alloc(
        FILE_TYPE_VNODE,
        &file_vnode_ops,
        so,
        flags
    );

    return f;
}

file_t *file_create_pipe([[maybe_unused]] pipe_t *pipe,
                         [[maybe_unused]] int flags,
                         [[maybe_unused]] bool writable_end)
{
    return NULL;
}

file_t *file_create_fifo([[maybe_unused]] fifo_t *vn,
                         [[maybe_unused]] int flags)
{
    return NULL;
}

file_t *file_create_msgq([[maybe_unused]] msgq_t *mq,
                         [[maybe_unused]] int flags)
{
    return NULL;
}

file_t *file_create_eventq([[maybe_unused]] eventq_t *eq,
                           [[maybe_unused]] int flags)
{
    return NULL;
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
