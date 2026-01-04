#include "sys/syscall.h"

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
    (void *)syscall_tcb_set,
    (void *)syscall_getcwd,
    (void *)syscall_chdir,
    (void *)syscall_mkdir,
    (void *)syscall_rmdir
};

const uint64_t syscall_table_length = sizeof(syscall_table) / sizeof(void*);

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
