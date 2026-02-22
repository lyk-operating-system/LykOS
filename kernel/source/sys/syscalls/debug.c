#include "sys/syscall.h"

#include "uapi/errno.h"
#include "log.h"

sys_ret_t syscall_debug_log(const char *str)
{
    log(LOG_DEBUG, "%s", str);

    return (sys_ret_t) {0, EOK};
}
