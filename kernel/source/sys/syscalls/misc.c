#include "sys/syscall.h"

#include "log.h"
#include "uapi/errno.h"

sys_ret_t debug_log(const char *s)
{
    log(LOG_DEBUG, s);
    return (sys_ret_t) {42, EOK};
}
