#include "proc/smp.h"
#include "proc/thread.h"
#include "sys/syscall.h"

#include "arch/misc.h"
#include "arch/timer.h"
#include "log.h"
#include "proc/sched.h"
#include "uapi/errno.h"
#include <stdint.h>

sys_ret_t syscall_exit(int code)
{
    log(LOG_DEBUG, "Process exited with code: %i.", code);

    sched_yield(THREAD_STATE_TERMINATED);

    unreachable();
}

sys_ret_t syscall_fork()
{
    proc_fork(sys_curr_proc());

    return (sys_ret_t) {0, EOK};
}

sys_ret_t syscall_get_cwd()
{
    return (sys_ret_t) {-1, EOK};
}

sys_ret_t syscall_get_pid()
{
    return (sys_ret_t) {sys_curr_proc()->pid, EOK};
}

sys_ret_t syscall_get_ppid()
{
    return (sys_ret_t) {sys_curr_proc()->ppid, EOK};
}

sys_ret_t syscall_get_tid()
{
    return (sys_ret_t) {sys_curr_thread()->tid, EOK};
}

sys_ret_t syscall_tcb_set(void *ptr)
{
    arch_syscall_tcb_set(ptr);

    return (sys_ret_t) {0, EOK};
}

// sleep in microseconds
sys_ret_t syscall_sleep(unsigned us)
{
    sys_curr_thread()->sleep_until = arch_timer_get_uptime_ns() + (uint64_t) us * 1000;
    sched_yield(THREAD_STATE_SLEEPING);
    return (sys_ret_t) {0, EOK};
}
