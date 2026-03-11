#include "syscall.h"

#include "log.h"
#include "sys/syscall.h"
#include "uapi/errno.h"

sys_ret_t syscall_accept(int sockfd, const char *addr)
{
    proc_t *proc = sys_curr_proc();

    file_t *file = fd_get(sys_curr_proc()->fd_table, sockfd);
    if (!file)
        return (sys_ret_t) {0, EBADF};



    return (sys_ret_t) {0, EOK};
}

sys_ret_t syscall_bind(int sockfd, const char *addr)
{
    proc_t *proc = sys_curr_proc();

    file_t *file = fd_get(sys_curr_proc()->fd_table, sockfd);
    if (!file)
        return (sys_ret_t) {0, EBADF};


    return (sys_ret_t) {0, EOK};
}

sys_ret_t syscall_connect(int sockfd, const char *addr)
{
    proc_t *proc = sys_curr_proc();

    file_t *file = fd_get(sys_curr_proc()->fd_table, sockfd);
    if (!file)
        return (sys_ret_t) {0, EBADF};


    return (sys_ret_t) {0, EOK};
}

sys_ret_t syscall_getpeername(int sockfd, char *addr)
{

}

sys_ret_t syscall_getsockname(int sockfd, char *addr)
{

}

sys_ret_t syscall_listen(int sockfd, int backlog)
{
    proc_t *proc = sys_curr_proc();

    file_t *file = fd_get(sys_curr_proc()->fd_table, sockfd);
    if (!file)
        return (sys_ret_t) {0, EBADF};


    return (sys_ret_t) {0, EOK};
}

sys_ret_t syscall_recv(int sockfd, void *buf, size_t len, int flags)
{

}

sys_ret_t syscall_send(int sockfd, const void *buf, size_t len, int flags)
{

}

sys_ret_t syscall_shutdown(int sockfd, int how)
{

}

sys_ret_t syscall_socket(int domain, int type, int protocol)
{
    proc_t *proc = sys_curr_proc();


    return (sys_ret_t) {0, EOK};
}
