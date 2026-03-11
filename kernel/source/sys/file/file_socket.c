#include "fs/vfs.h"
#include "sys/file.h"
#include "sys/poll.h"
#include "sys/uio.h"
#include "sys/unistd.h"
#include "uapi/errno.h"

static int file_socket_read(file_t *fp, uio_op_t *uio,
                           [[maybe_unused]] int flags,
                           [[maybe_unused]] thread_t *td)
{
    return 0;
}

static int file_socket_write(file_t *fp, uio_op_t *uio,
                            [[maybe_unused]] int flags,
                            [[maybe_unused]] thread_t *td)
{
    return 0;
}

static int file_socket_ioctl(file_t *fp, int cmd, void *data, [[maybe_unused]] thread_t *td)
{
    return 0;
}

static int file_socket_poll([[maybe_unused]] file_t *fp, int events,
                           [[maybe_unused]] thread_t *td)
{
    return 0;
}

static int file_socket_truncate([[maybe_unused]] file_t *fp,
                               [[maybe_unused]] off_t length,
                               [[maybe_unused]] thread_t *td)
{
    return 0;
}

static int file_socket_chmod([[maybe_unused]] file_t *fp,
                            [[maybe_unused]] mode_t mode,
                            [[maybe_unused]] thread_t *td)
{
    return 0;
}

static int file_socket_chown([[maybe_unused]] file_t *fp,
                            [[maybe_unused]] uid_t uid,
                            [[maybe_unused]] gid_t gid,
                            [[maybe_unused]] thread_t *td)
{
    return 0;
}

static int file_socket_seek(file_t *fp, off_t offset, int whence)
{
    return 0;
}

static int file_socket_close(file_t *fp, [[maybe_unused]] thread_t *td)
{
    return 0;
}

const file_ops_t file_socket_ops = {
    .read     = file_socket_read,
    .write    = file_socket_write,
    .ioctl    = file_socket_ioctl,
    .poll     = file_socket_poll,
    .truncate = file_socket_truncate,
    .chmod    = file_socket_chmod,
    .chown    = file_socket_chown,
    .seek     = file_socket_seek,
    .close    = file_socket_close,
};
