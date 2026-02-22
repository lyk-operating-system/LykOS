#pragma once

#include "sync/spinlock.h"
#include "sys/types.h"
#include "utils/ref.h"

/*
 * Forward declarations
 */

typedef struct file file_t;
typedef struct file_ops file_ops_t;
typedef struct thread thread_t;
typedef struct uio_op uio_op_t;
typedef struct vnode vnode_t;

/*
 * File types
 */

typedef enum file_type
{
    FILE_TYPE_NONE,   // not yet initialized
    FILE_TYPE_VNODE,  // normal file from FS
    FILE_TYPE_SOCKET,
    FILE_TYPE_PIPE,
    FILE_TYPE_NAMED_PIPE,
    FILE_TYPE_EVENT_QUEUE,
    FILE_TYPE_MESSAGE_QUEUE
}
file_type_t;

/*
 * Open file structure
 */

struct file
{
    file_type_t type;
    const file_ops_t *ops;
    void *data; // backing vnode/socket/etc.
    int flags;
    size_t offset;

    ref_t refcount;
    spinlock_t slock;
};

/*
 * Open file operations
 */

typedef int file_read_t(file_t *fp, uio_op_t *uio_op, int flags, thread_t *td);

typedef int file_write_t(file_t *fp, uio_op_t *uio_op, int flags, thread_t *td);

typedef int file_ioctl_t(file_t *fp, int cmd, void *data, thread_t *td);

typedef int file_poll_t(file_t *fp, int events, thread_t *td);

typedef int file_truncate_t(file_t *fp, off_t length, thread_t *td);

typedef int file_chmod_t(file_t *fp, mode_t mode, thread_t *td);

typedef int file_chown_t(file_t *fp, uid_t uid, gid_t gid, thread_t *td);

typedef int file_seek_t(file_t *fp, off_t offset, int whence);

typedef int file_close_t(file_t *fp, thread_t *td);

struct file_ops
{
    file_read_t      *read;
    file_write_t     *write;
    file_ioctl_t     *ioctl;
    file_poll_t      *poll;
    file_truncate_t  *truncate;
    file_chmod_t     *chmod;
    file_chown_t     *chown;
    file_seek_t      *seek;
    file_close_t     *close;
};

/*
 * Public API
 */

file_t *file_create_vnode(vnode_t *vn, int flags);
void file_hold(file_t *file);
void file_drop(file_t *file);
