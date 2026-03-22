#include "syscall.h"

#include "assert.h"
#include "log.h"
#include "sys/file.h"
#include "sys/socket.h"
#include "sys/syscall.h"
#include "uapi/errno.h"
#include "utils/math.h"
#include <stdint.h>

sys_ret_t syscall_accept(int sockfd, const struct sockaddr *addr, socklen_t addr_len, int flags)
{
    int err = EOK;

    file_t *file = NULL;
    file_t *new_file = NULL;
    socket_t *client = NULL;

    file = fd_get_file(sys_curr_proc()->fd_table, sockfd);
    if (!file)
    {
        err = EBADF;
        goto fail;
    }

    if (file->type != FILE_TYPE_SOCKET)
    {
        err = ENOTSOCK;
        goto fail;
    }

    socket_t *so = file->backend;

    err = so->ops->accept(so, addr, addr_len, flags, &client);
    if (err != EOK)
    {
        log(LOG_WARN, "A");
        goto fail;
    }

    new_file = file_create_socket(client, 0);
    if (!new_file)
    {
        err = ENOMEM;
        goto fail;
    }

    int fd;
    err = fd_alloc(sys_curr_proc()->fd_table, new_file, &fd);
    if (err != EOK)
        goto fail;

    file_unref(file);
    file_unref(new_file); // fd_alloc will hold a ref
    return (sys_ret_t) {fd, EOK};

fail:
    log(LOG_WARN, "accept err %d", err);
    if (new_file) file_unref(new_file);
    if (file) file_unref(file);
    return (sys_ret_t) {0, err};
}

sys_ret_t syscall_bind(int sockfd, const struct sockaddr *addr)
{
    int err = EOK;

    file_t *file = NULL;
    socket_t *so = NULL;

    file = fd_get_file(sys_curr_proc()->fd_table, sockfd);
    if (!file)
    {
        err = EBADF;
        goto fail;
    }

    if (file->type != FILE_TYPE_SOCKET)
    {
        err = ENOTSOCK;
        goto fail;
    }

    so = file->backend;

    err = so->ops->bind(so, addr);
    if (err != EOK)
        goto fail;

    file_unref(file);
    return (sys_ret_t) {0, EOK};

fail:
    if (file) file_unref(file);
    return (sys_ret_t) {0, err};
}

sys_ret_t syscall_connect(int sockfd, const struct sockaddr *addr)
{
    int err = EOK;

    file_t *file = NULL;
    socket_t *so = NULL;

    file = fd_get_file(sys_curr_proc()->fd_table, sockfd);
    if (!file)
    {
        err = EBADF;
        goto fail;
    }

    if (file->type != FILE_TYPE_SOCKET)
    {
        err = ENOTSOCK;
        goto fail;
    }

    so = file->backend;

    err = so->ops->connect(so, addr);
    if (err != EOK)
        goto fail;

    file_unref(file);
    return (sys_ret_t) {0, EOK};

fail:
    if (file) file_unref(file);
    return (sys_ret_t) {0, err};
}

sys_ret_t syscall_getpeername(int sockfd, struct sockaddr *addr, socklen_t length)
{
    file_t *file = fd_get_file(sys_curr_proc()->fd_table, sockfd);
    if (!file)
        return (sys_ret_t) {0, EBADF};

    if (file->type != FILE_TYPE_SOCKET)
    {
        file_unref(file);
        return (sys_ret_t) {0, ENOTSOCK};
    }

    socket_t *so = file->backend;

    struct sockaddr ret;
    int err = so->ops->getpeername(so, &ret);
    if (err != EOK)
    {
        file_unref(file);
        return (sys_ret_t) {0, err};
    }

    size_t copy = MIN(length, sizeof(struct sockaddr));
    vm_copy_to_user(
        sys_curr_as(), (uintptr_t)addr,
        &ret, copy
    );
    file_unref(file);
    return (sys_ret_t) {copy, EOK};
}

sys_ret_t syscall_getsockname(int sockfd, struct sockaddr *addr, socklen_t length)
{
    file_t *file = fd_get_file(sys_curr_proc()->fd_table, sockfd);
    if (!file)
        return (sys_ret_t) {0, EBADF};

    if (file->type != FILE_TYPE_SOCKET)
    {
        file_unref(file);
        return (sys_ret_t) {0, ENOTSOCK};
    }

    socket_t *so = file->backend;

    struct sockaddr ret;
    int err = so->ops->getsockname(so, &ret);
    if (err != EOK)
    {
        file_unref(file);
        return (sys_ret_t) {0, err};
    }

    size_t copy = MIN(length, sizeof(struct sockaddr));
    vm_copy_to_user(
        sys_curr_as(), (uintptr_t)addr,
        &ret, copy
    );

    file_unref(file);
    return (sys_ret_t) {copy, EOK};
}

sys_ret_t syscall_listen(int sockfd, int backlog)
{
    int err = EOK;

    file_t *file = NULL;
    socket_t *so = NULL;

    file = fd_get_file(sys_curr_proc()->fd_table, sockfd);
    if (!file)
    {
        err = EBADF;
        goto fail;
    }

    if (file->type != FILE_TYPE_SOCKET)
    {
        err = ENOTSOCK;
        goto fail;
    }

    so = file->backend;

    err = so->ops->listen(so, backlog);
    if (err != EOK)
        goto fail;

    file_unref(file);
    return (sys_ret_t) {0, EOK};

fail:
    if (file) file_unref(file);
    return (sys_ret_t) {0, err};
}

sys_ret_t syscall_recv(int sockfd, void *buf, size_t len, int flags)
{
    int err = EOK;
    ssize_t ret = 0;

    file_t *file = NULL;
    socket_t *so = NULL;

    file = fd_get_file(sys_curr_proc()->fd_table, sockfd);
    if (!file)
    {
        err = EBADF;
        goto fail;
    }

    if (file->type != FILE_TYPE_SOCKET)
    {
        err = ENOTSOCK;
        goto fail;
    }

    so = file->backend;

    uint64_t recv_bytes = 0;
    ret = so->ops->recv(so, buf, len, flags, sys_curr_thread(), &recv_bytes);
    if (ret != EOK)
    {
        err = ret;
        goto fail;
    }

    file_unref(file);
    return (sys_ret_t) {recv_bytes, EOK};

fail:
    if (file) file_unref(file);
    return (sys_ret_t) {0, err};
}

sys_ret_t syscall_send(int sockfd, const void *buf, size_t len, int flags)
{
    int err = EOK;
    ssize_t ret = 0;

    file_t *file = NULL;
    socket_t *so = NULL;

    file = fd_get_file(sys_curr_proc()->fd_table, sockfd);
    if (!file)
    {
        err = EBADF;
        goto fail;
    }

    if (file->type != FILE_TYPE_SOCKET)
    {
        err = ENOTSOCK;
        goto fail;
    }

    so = file->backend;

    uint64_t sent_bytes = 0;
    ret = so->ops->send(so, buf, len, flags, sys_curr_thread(), &sent_bytes);
    if (ret != EOK)
    {
        err = ret;
        goto fail;
    }

    file_unref(file);
    return (sys_ret_t) {sent_bytes, EOK};

fail:
    if (file) file_unref(file);
    return (sys_ret_t) {0, err};
}

sys_ret_t syscall_shutdown(int sockfd, int how)
{
    file_t *file = fd_get_file(sys_curr_proc()->fd_table, sockfd);
    if (!file)
        return (sys_ret_t) {0, EBADF};

    if (file->type != FILE_TYPE_SOCKET)
    {
        file_unref(file);
        return (sys_ret_t) {0, ENOTSOCK};
    }

    socket_t *so = file->backend;
    int err = so->ops->shutdown(so, how);

    file_unref(file);
    return (sys_ret_t) {0, err};
}

sys_ret_t syscall_socket(int domain, int type, int protocol)
{
    int err = EOK;

    socket_t *so = NULL;
    file_t *file = NULL;

    err = socket_create(domain, type, protocol, &so);
    if (err != EOK)
        goto fail;

    file = file_create_socket(so, 0);
    if (!file)
    {
        err = ENOMEM;
        goto fail;
    }

    int fd;
    err = fd_alloc(sys_curr_proc()->fd_table, file, &fd);
    if (err != EOK)
        goto fail;

    file_unref(file); // fd table owns the ref now
    return (sys_ret_t) {fd, EOK};

fail:
    if (file) file_unref(file);
    else if (so) socket_destroy(so);

    return (sys_ret_t) {0, err};
}
