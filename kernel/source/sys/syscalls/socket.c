#include "syscall.h"

#include "log.h"
#include "sys/file.h"
#include "sys/socket.h"
#include "sys/syscall.h"
#include "uapi/errno.h"

sys_ret_t syscall_accept(int sockfd, const char *addr)
{
    file_t *file = fd_get(sys_curr_proc()->fd_table, sockfd);
    if (!file)
        return (sys_ret_t) {0, EBADF};

    if (file->type != FILE_TYPE_SOCKET)
    {
        fd_put(file);
        return (sys_ret_t) {0, ENOTSOCK};
    }

    socket_t *so = file->backend;

    return (sys_ret_t) {0, EOK};
}

sys_ret_t syscall_bind(int sockfd, const char *addr)
{
    file_t *file = fd_get(sys_curr_proc()->fd_table, sockfd);
    if (!file)
        return (sys_ret_t) {0, EBADF};

    if (file->type != FILE_TYPE_SOCKET)
    {
        fd_put(file);
        return (sys_ret_t) {0, ENOTSOCK};
    }

    socket_t *so = file->backend;


    return (sys_ret_t) {0, EOK};
}

sys_ret_t syscall_connect(int sockfd, const char *addr)
{
    file_t *file = fd_get(sys_curr_proc()->fd_table, sockfd);
    if (!file)
        return (sys_ret_t) {0, EBADF};

    if (file->type != FILE_TYPE_SOCKET)
    {
        fd_put(file);
        return (sys_ret_t) {0, ENOTSOCK};
    }

    socket_t *so = file->backend;


    return (sys_ret_t) {0, EOK};
}

sys_ret_t syscall_getpeername(int sockfd, char *addr)
{
    file_t *file = fd_get(sys_curr_proc()->fd_table, sockfd);
    if (!file)
        return (sys_ret_t) {0, EBADF};

    if (file->type != FILE_TYPE_SOCKET)
    {
        fd_put(file);
        return (sys_ret_t) {0, ENOTSOCK};
    }

    socket_t *so = file->backend;

}

sys_ret_t syscall_getsockname(int sockfd, char *addr)
{
    file_t *file = fd_get(sys_curr_proc()->fd_table, sockfd);
    if (!file)
        return (sys_ret_t) {0, EBADF};

    if (file->type != FILE_TYPE_SOCKET)
    {
        fd_put(file);
        return (sys_ret_t) {0, ENOTSOCK};
    }

    socket_t *so = file->backend;
}

sys_ret_t syscall_listen(int sockfd, int backlog)
{
    file_t *file = fd_get(sys_curr_proc()->fd_table, sockfd);
    if (!file)
        return (sys_ret_t) {0, EBADF};

    if (file->type != FILE_TYPE_SOCKET)
    {
        fd_put(file);
        return (sys_ret_t) {0, ENOTSOCK};
    }

    socket_t *so = file->backend;

    return (sys_ret_t) {0, EOK};
}

sys_ret_t syscall_recv(int sockfd, void *buf, size_t len, int flags)
{
    file_t *file = fd_get(sys_curr_proc()->fd_table, sockfd);
    if (!file)
        return (sys_ret_t) {0, EBADF};

    if (file->type != FILE_TYPE_SOCKET)
    {
        fd_put(file);
        return (sys_ret_t) {0, ENOTSOCK};
    }

    socket_t *so = file->backend;
}

sys_ret_t syscall_send(int sockfd, const void *buf, size_t len, int flags)
{
    file_t *file = fd_get(sys_curr_proc()->fd_table, sockfd);
    if (!file)
        return (sys_ret_t) {0, EBADF};

    if (file->type != FILE_TYPE_SOCKET)
    {
        fd_put(file);
        return (sys_ret_t) {0, ENOTSOCK};
    }

    socket_t *so = file->backend;
}

sys_ret_t syscall_shutdown(int sockfd, int how)
{
    file_t *file = fd_get(sys_curr_proc()->fd_table, sockfd);
    if (!file)
        return (sys_ret_t) {0, EBADF};

    if (file->type != FILE_TYPE_SOCKET)
    {
        fd_put(file);
        return (sys_ret_t) {0, ENOTSOCK};
    }
}

sys_ret_t syscall_socket(int domain, int type, int protocol)
{
    socket_t *so;
    int err = socket_create(domain, type, protocol, &so);
    if (!so)
        return (sys_ret_t) {0, err};

    file_t *file = file_create_socket(so, 0);
    if (!file)
    {

        return (sys_ret_t){0, ENOMEM};
    }


    return (sys_ret_t) {0, EOK};
}
