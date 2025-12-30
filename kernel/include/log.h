#pragma once

#include <stdarg.h>
#include "include/arch/clock.h"

typedef enum
{
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL
}
log_level_t;

void vlog(log_level_t level, const char *format, va_list vargs);

void log(log_level_t level, const char *format, ...);
