#pragma once

#include <stdarg.h>

typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL
} log_level_t;

void vlog(log_level_t level, const char *component, const char *format, va_list vargs);
void _log(log_level_t level, const char *component, const char *format, ...);


#ifdef LOG_PREFIX
#define log(LEVEL, FORMAT, ...) \
    _log(LEVEL, LOG_PREFIX, FORMAT, ##__VA_ARGS__)
#else
#define log(LEVEL, FORMAT, ...) \
    _log(LEVEL, __FILE_NAME__, FORMAT, ##__VA_ARGS__)
#endif