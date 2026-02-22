#include "sys/fd.h"
#include "sys/syscall.h"

#include "fs/vfs.h"
#include "log.h"
#include "sys/file.h"
#include "sys/proc.h"
#include "sys/thread.h"
#include "sys/uio.h"
#include "uapi/errno.h"

// Access mode
#define O_RDONLY    0x00000
#define O_WRONLY    0x00001
#define O_RDWR      0x00002
#define O_EXEC      0x00003
#define O_SEARCH    0x00004

#define O_ACCMODE   0x00007

// Creation
#define O_CREAT     0x00008
#define O_EXCL      0x00010
#define O_TRUNC     0x00020
#define O_DIRECTORY 0x00040
#define O_NOFOLLOW  0x00080
#define O_NOCTTY    0x00100
#define O_TTY_INIT  0x00200

// FD behavior
#define O_CLOEXEC   0x00400
#define O_CLOFORK   0x00800

// IO behavior
#define O_APPEND    0x01000
#define O_NONBLOCK  0x02000
#define O_SYNC      0x04000
#define O_DSYNC     0x08000
#define O_RSYNC     0x10000

// Seek
#define SEEK_SET    0x0
#define SEEK_CUR    0x1
#define SEEK_END    0x2
#define SEEK_HOLE   0x4
#define SEEK_DATA   0x8

sys_ret_t syscall_open(const char *path, int flags)
{
    char kpath[1024];
    vm_copy_from_user(sys_curr_as(), kpath, (uintptr_t)path, sizeof(kpath) - 1);
    kpath[sizeof(kpath) - 1] = '\0';

    vnode_t *vn;
    int err = vfs_lookup(kpath, &vn);

    if (err != EOK)
    {
        if ((flags & O_CREAT) == 0)
            return (sys_ret_t) {0, err};

        err = vfs_create(kpath, VREG, &vn);
        if (err != EOK)
            return (sys_ret_t) {0, err};
    }

    file_t *file = file_create_vnode(vn, flags);
    if (!file)
    {
        vnode_unref(vn);
        return (sys_ret_t){0, ENOMEM};
    }

    int fd;
    err = fd_alloc(sys_curr_proc()->fd_table, file, &fd);
    if (err != EOK)
    {
        file_drop(file);
        return (sys_ret_t) {0, err};
    }

    file_drop(file); // the fd table holds a ref (because of fd_alloc), drop ours
    return (sys_ret_t) {fd, EOK};
}

sys_ret_t syscall_close(int fd)
{
    return (sys_ret_t) {0, fd_free(sys_curr_proc()->fd_table, fd)};
}

sys_ret_t syscall_read(int fd, void *buf, size_t count)
{
    file_t *file = fd_get(sys_curr_proc()->fd_table, fd);
    if (!file)
        return (sys_ret_t){0, EBADF};

    if (!file->ops || !file->ops->read)
    {
        fd_put(file);
        return (sys_ret_t) {0, EBADF};
    }

    uio_op_buf_t uio_buf = {
        .base   = buf,
        .length = count
    };

    uio_op_t uio_op = {
        .type      = UIO_OP_READ,
        .buf       = &uio_buf,
        .buf_cnt   = 1,
        .offset    = file->offset,
        .rem_bytes = count,
        .thread    = sys_curr_thread()
    };

    int err = file->ops->read(file, &uio_op, file->flags,
                                sys_curr_thread());
    if (err == EOK)
    {
        size_t read_bytes = count - uio_op.rem_bytes;
        fd_put(file);
        return (sys_ret_t){read_bytes, EOK};
    }

    fd_put(file);
    return (sys_ret_t){0, err};
}

sys_ret_t syscall_seek(int fd, uint64_t offset, int whence)
{
    file_t *file = fd_get(sys_curr_proc()->fd_table, fd);
    if (!file)
        return (sys_ret_t) {0, EBADF};

    if (!file->ops || !file->ops->seek)
    {
        fd_put(file);
        return (sys_ret_t) {0, ESPIPE};
    }

    int err = file->ops->seek(file, (off_t)offset, whence);
    if (err != EOK)
    {
        fd_put(file);
        return (sys_ret_t){0, err};
    }

    fd_put(file);
    return (sys_ret_t){file->offset, EOK};
}

sys_ret_t syscall_write(int fd, void *buf, size_t count)
{
    file_t *file = fd_get(sys_curr_proc()->fd_table, fd);
    if (!file)
        return (sys_ret_t) {0, EBADF};

    if (!file->ops || !file->ops->write)
    {
        fd_put(file);
        return (sys_ret_t) {0, EBADF};
    }

    uio_op_buf_t op_buf = {
        .base   = buf,
        .length = count
    };

    uio_op_t uio_op = {
        .type      = UIO_OP_WRITE,
        .buf       = &op_buf,
        .buf_cnt   = 1,
        .offset    = file->offset,
        .rem_bytes = count,
        .thread    = sys_curr_thread()
    };

    int err = file->ops->write(file, &uio_op, file->flags,
                                 sys_curr_thread());
    if (err == EOK)
    {
        fd_put(file);
        return (sys_ret_t) {count - uio_op.rem_bytes, EOK};
    }

    fd_put(file);
    return (sys_ret_t) {0, err};
}
