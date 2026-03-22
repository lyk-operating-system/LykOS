#include "sys/socket.h"
#include "sys/file.h"
#include "sys/poll.h"
#include "sys/uio.h"
#include "sys/unistd.h"
#include "uapi/errno.h"

static int file_socket_read(file_t *fp, uio_op_t *uio,
                           [[maybe_unused]] int flags,
                           thread_t *t)
{
    socket_t *so = fp->backend;
    int error = 0;

    for (size_t i = 0; i < uio->buf_cnt && uio->rem_bytes > 0; i++)
    {
        uio_op_buf_t *b = &uio->buf[i];

        if (b->length == 0)
            continue;

        uint64_t to_read = b->length;
        if (to_read > (uint64_t)uio->rem_bytes)
            to_read = uio->rem_bytes;

        uint64_t recv_bytes = 0;
        error = so->ops->recv(
            so,
            b->base,
            to_read,
            flags,
            t,
            &recv_bytes
        );
        if (error)
            return error;

        fp->offset     += recv_bytes;
        uio->offset    += recv_bytes;
        uio->rem_bytes -= recv_bytes;

        if (recv_bytes < to_read)
            break; // EOF
    }

    return EOK;
}

static int file_socket_write(file_t *fp, uio_op_t *uio,
                            [[maybe_unused]] int flags,
                            thread_t *t)
{
    socket_t *so = fp->backend;
    int error = 0;

    uint64_t offset = fp->offset;

    for (size_t i = 0; i < uio->buf_cnt && uio->rem_bytes > 0; i++)
    {
        uio_op_buf_t *b = &uio->buf[i];

        if (b->length == 0)
            continue;

        uint64_t to_write = b->length;
        if (to_write > (uint64_t)uio->rem_bytes)
            to_write = uio->rem_bytes;

        uint64_t sent_bytes = 0;
        error = so->ops->send(
            so,
            b->base,
            to_write,
            flags,
            t,
            &sent_bytes
        );
        if (error)
            return error;

        offset          += sent_bytes;
        uio->offset     += sent_bytes;
        uio->rem_bytes  -= sent_bytes;

        if (sent_bytes < to_write)
            break;
    }

    fp->offset = offset;

    return 0;
}

static int file_socket_ioctl(file_t *fp, int cmd, void *data, [[maybe_unused]] thread_t *t)
{
    return 0;
}

static int file_socket_poll([[maybe_unused]] file_t *fp, int events,
                           [[maybe_unused]] thread_t *t)
{
    return 0;
}

static int file_socket_truncate([[maybe_unused]] file_t *fp,
                               [[maybe_unused]] off_t length,
                               [[maybe_unused]] thread_t *t)
{
    return 0;
}

static int file_socket_chmod([[maybe_unused]] file_t *fp,
                            [[maybe_unused]] mode_t mode,
                            [[maybe_unused]] thread_t *t)
{
    return 0;
}

static int file_socket_chown([[maybe_unused]] file_t *fp,
                            [[maybe_unused]] uid_t uid,
                            [[maybe_unused]] gid_t gid,
                            [[maybe_unused]] thread_t *t)
{
    return 0;
}

static int file_socket_seek(file_t *fp, off_t offset, int whence)
{
    return 0;
}

static int file_socket_close(file_t *fp, [[maybe_unused]] thread_t *t)
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
