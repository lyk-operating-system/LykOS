#pragma once

#include <stddef.h>
#include <stdint.h>

#include "mm/vm.h"
#include "sys/proc.h"
#include "sys/thread.h"

/*
 * Helpers
 */

proc_t *sys_curr_proc();

thread_t *sys_curr_thread();

vm_addrspace_t *sys_curr_as();

/*
 * Return value
 */

typedef struct
{
    size_t value;
    size_t error;
}
__attribute__((packed))
sys_ret_t;

/*
 * Debug
 */

sys_ret_t syscall_debug_log(const char *s);

/*
 * File IO
 */

sys_ret_t syscall_open(const char *path, int flags);
sys_ret_t syscall_close(int fd);
sys_ret_t syscall_read(int fd, void *buf, size_t count);
sys_ret_t syscall_write(int fd, void *buf, size_t count);
sys_ret_t syscall_seek(int fd, size_t offset, int whence);

/*
 * Memory
 */

sys_ret_t syscall_mmap(uintptr_t addr, size_t len, int prot, int flags, int fd, size_t off);

/*
 * Process
 */

sys_ret_t syscall_exit(int code);
sys_ret_t syscall_fork();
sys_ret_t syscall_get_cwd();
sys_ret_t syscall_get_pid();
sys_ret_t syscall_get_ppid();
sys_ret_t syscall_get_tid();
sys_ret_t syscall_tcb_set(void *ptr);

/*
 * Sockets
 */

struct sockaddr;
typedef uint32_t socklen_t;

sys_ret_t syscall_accept(int sockfd, const struct sockaddr *addr, socklen_t addr_len, int flags);
sys_ret_t syscall_bind(int sockfd, const struct sockaddr *addr);
sys_ret_t syscall_connect(int sockfd, const struct sockaddr *addr);
sys_ret_t syscall_getpeername(int sockfd, struct sockaddr *addr, socklen_t length);
sys_ret_t syscall_getsockname(int sockfd, struct sockaddr *addr, socklen_t length);
sys_ret_t syscall_listen(int sockfd, int backlog);
sys_ret_t syscall_recv(int sockfd, void *buf, size_t len, int flags);
sys_ret_t syscall_send(int sockfd, const void *buf, size_t len, int flags);
sys_ret_t syscall_shutdown(int sockfd, int how);
sys_ret_t syscall_socket(int domain, int type, int protocol);
