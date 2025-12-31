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
void _log(log_level_t level, const char *component, const char *file, const char *format, ...);

#ifdef LOG_PREFIX
    #define LOG_COMPONENT LOG_PREFIX
#else
    #define LOG_COMPONENT NULL
#endif

#define log(LEVEL, FORMAT, ...) _log(LEVEL, LOG_COMPONENT, __FILE__, FORMAT, ##__VA_ARGS__)
