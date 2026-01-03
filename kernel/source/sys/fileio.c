#include "sys/syscall.h"

#include "proc/proc.h"
#include "proc/sched.h"
#include "proc/thread.h"
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

        // err = vfs_create(NULL, kpath, VREG, &vn);
        if (err != EOK)
            return (sys_ret_t) {0, err};
    }

    int fd;
    if (!fd_alloc(&sys_curr_proc()->fd_table, vn, &fd))
    {
        vnode_unref(vn);
        return (sys_ret_t){0, EMFILE};
    }

    // fd_alloc takes ownership of the vnode ref
    vnode_unref(vn);
    return (sys_ret_t) {fd, EOK};
}

sys_ret_t syscall_close(int fd)
{
    return (sys_ret_t) {0, fd_free(&sys_curr_proc()->fd_table, fd) ? EOK : EBADF};
}

sys_ret_t syscall_read(int fd, void *buf, uint64_t count)
{
    fd_entry_t *fd_entry = fd_get(&sys_curr_proc()->fd_table, fd);
    if (fd_entry == NULL)
        return (sys_ret_t) {0, EBADF};

    uint64_t read_bytes;
    int err = vfs_read(fd_entry->vnode, buf, fd_entry->offset, count, &read_bytes);

    fd_put(fd_entry);

    if (err != EOK)
        return (sys_ret_t) {0, err};
    return (sys_ret_t) {read_bytes, EOK};
}

#define SEEK_SET    0x0
#define SEEK_CUR    0x1
#define SEEK_END    0x2
#define SEEK_HOLE   0x4
#define SEEK_DATA   0x8

sys_ret_t syscall_seek(int fd, uint64_t offset, int whence)
{
    fd_entry_t *entry = fd_get(&sys_curr_proc()->fd_table, fd);
    if (entry == NULL)
        return (sys_ret_t){0, EBADF};

    uint64_t new_off;

    switch (whence)
    {
        case SEEK_SET:
            new_off = offset;
            break;
        case SEEK_CUR:
            new_off = entry->offset + offset;
            break;
        case SEEK_END:
            new_off = entry->vnode->size + offset;
            break;
        case SEEK_HOLE:
        case SEEK_DATA:
            fd_put(entry);
            return (sys_ret_t) {0, ENOTSUP};
        default:
            fd_put(entry);
            return (sys_ret_t) {0, EINVAL};
    }

    entry->offset = new_off;
    fd_put(entry);

    return (sys_ret_t) {new_off, EOK};
}

sys_ret_t syscall_write(int fd, void *buf, uint64_t count)
{
    fd_entry_t *fd_entry = fd_get(&sys_curr_proc()->fd_table, fd);
    if (fd_entry == NULL)
        return (sys_ret_t) {0, EBADF};

    uint64_t written_bytes;
    int err = vfs_write(fd_entry->vnode, buf, fd_entry->offset, count, &written_bytes);

    fd_put(fd_entry);

    if (err != EOK)
        return (sys_ret_t) {0, err};
    return (sys_ret_t) {written_bytes, EOK};
}
