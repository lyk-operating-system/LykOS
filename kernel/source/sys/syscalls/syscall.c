#include "syscall.h"

#include "proc/sched.h"

const void* syscall_table[] = {
    (void *)syscall_debug_log,
    (void *)syscall_open,
    (void *)syscall_close,
    (void *)syscall_read,
    (void *)syscall_write,
    (void *)syscall_seek,
    (void *)syscall_mmap,
    (void *)syscall_exit,
    (void *)syscall_fork,
    (void *)syscall_get_cwd,
    (void *)syscall_get_pid,
    (void *)syscall_get_ppid,
    (void *)syscall_get_tid,
    (void *)syscall_tcb_set,
};

const uint64_t syscall_table_length = sizeof(syscall_table) / sizeof(void *);

// Helpers

proc_t *sys_curr_proc()
{
    return sched_get_curr_thread()->owner;
}

thread_t *sys_curr_thread()
{
    return sched_get_curr_thread();
}

vm_addrspace_t *sys_curr_as()
{
    return sched_get_curr_thread()->owner->as;
}
