#include "sys/syscall.h"

#include "arch/misc.h"
#include "log.h"
#include "proc/sched.h"

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
