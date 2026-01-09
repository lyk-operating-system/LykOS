#include "proc/smp.h"
#include "proc/thread.h"
#include "sys/syscall.h"

#include "arch/misc.h"
#include "arch/timer.h"
#include "log.h"
#include "proc/sched.h"
#include <stdint.h>

sys_ret_t syscall_exit(int code)
{
    log(LOG_DEBUG, "Process exited with code: %i.", code);

    while (true)
        ;

    //sched_yield(THREAD_STATE_AWAITING_CLEANUP);

    unreachable();
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
