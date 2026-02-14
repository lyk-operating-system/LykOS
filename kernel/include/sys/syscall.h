#pragma once

#include <stddef.h>
#include <stdint.h>

#include "mm/vm.h"
#include "proc/proc.h"
#include "proc/thread.h"

// Helpers

proc_t *sys_curr_proc();

thread_t *sys_curr_thread();

vm_addrspace_t *sys_curr_as();

// System calls

typedef struct
{
    size_t value;
    size_t error;
}
__attribute__((packed))
sys_ret_t;

// DEBUG

sys_ret_t syscall_debug_log(const char *s);

// IO

sys_ret_t syscall_open(const char *path, int flags);
sys_ret_t syscall_close(int fd);
sys_ret_t syscall_read(int fd, void *buf, size_t count);
sys_ret_t syscall_write(int fd, void *buf, size_t count);
sys_ret_t syscall_seek(int fd, size_t offset, int whence);

// Memory

sys_ret_t syscall_mmap(uintptr_t addr, size_t len, int prot, int flags, int fd, size_t off);

// Process

sys_ret_t syscall_exit(int code);
sys_ret_t syscall_fork();
sys_ret_t syscall_get_cwd();
sys_ret_t syscall_get_pid();
sys_ret_t syscall_get_ppid();
sys_ret_t syscall_get_tid();
sys_ret_t syscall_tcb_set(void *ptr);
