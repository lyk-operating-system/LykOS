#include "log.h"

#include "arch/serial.h"
#include "gfx/console.h"
#include "arch/clock.h"
#include "utils/string.h"
#include "utils/printf.h"
#include "sync/spinlock.h"

static spinlock_t slock = SPINLOCK_INIT;

static const char *level_to_string(log_level_t level)
{
    static const char *names[] = {"DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
    return names[level];
}

static void to_upper(char *str)
{
    while (*str)
    {
        if (*str >= 'a' && *str <= 'z')
            *str = *str - 'a' + 'A';
        str++;
    }
}

void vlog(log_level_t level, const char *component, const char *format, va_list vargs)
{
    char msg[512];
    vsnprintf(msg, sizeof(msg), format, vargs);

    char out[1024];
    arch_clock_snapshot_t now;
    if (arch_clock_get_snapshot(&now))
    {
        if (component)
        {
            snprintf(out, sizeof(out),
                     "[%02u:%02u:%02u|%5s|%s] %s",
                     now.hour,
                     now.min,
                     now.sec,
                     level_to_string(level),
                     component,
                     msg);
        }
        else
        {
            snprintf(out, sizeof(out),
                     "[%02u:%02u:%02u|%5s] %s",
                     now.hour,
                     now.min,
                     now.sec,
                     level_to_string(level),
                     msg);
        }
    }
    else
        snprintf(out, sizeof(out), "%s", msg);

    spinlock_acquire(&slock);
    arch_serial_write(out);
    arch_serial_write("\n");
    console_write(out);
    spinlock_release(&slock);
}

void _log(log_level_t level, const char *component, const char *format, ...)
{
    va_list vargs;
    va_start(vargs, format);

    char comp_name[64];
    strcpy(comp_name, component);
    char *p = strstr(comp_name, ".c");
    if (p)
    {
        *p = '\0';
        to_upper(comp_name);
    }
    vlog(level, comp_name, format, vargs);

    va_end(vargs);
}
