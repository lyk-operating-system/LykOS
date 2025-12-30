#include "log.h"

#include "arch/serial.h"
#include "gfx/console.h"
#include "utils/printf.h"
#include "sync/spinlock.h"

static spinlock_t slock = SPINLOCK_INIT;

static const char *month_names[] = {
    "Jan","Feb","Mar","Apr","May","Jun",
    "Jul","Aug","Sep","Oct","Nov","Dec"
};

void vlog(log_level_t level, const char *format, va_list vargs)
{
    (void)level;

    char msg[256];
    vsnprintf(msg, sizeof(msg), format, vargs);

    char out[320];
    arch_clock_snapshot_t now;
    if (arch_clock_get_snapshot(&now)) 
    {
        snprintf(out, sizeof(out),
            "%s %02u %02u:%02u:%02u %s",
            month_names[now.month - 1],
            now.day,
            now.hour,
            now.min,
            now.sec,
            msg
        );
    } 
    else 
    {
        snprintf(out, sizeof(out), "%s", msg);
    }

    spinlock_acquire(&slock);

    arch_serial_write(out);
    arch_serial_write("\n");

    console_write(out);

    spinlock_release(&slock);
}

void log(log_level_t level, const char *format, ...)
{
    va_list vargs;
    va_start(vargs);

    vlog(level, format, vargs);

    va_end(vargs);
}
