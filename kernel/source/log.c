#include "log.h"

#include "arch/serial.h"
#include "gfx/console.h"
#include "include/arch/clock.h"
#include "include/utils/string.h"
#include "utils/printf.h"
#include "sync/spinlock.h"

static spinlock_t slock = SPINLOCK_INIT;

static const char *month_names[] = {
    "Jan","Feb","Mar","Apr","May","Jun",
    "Jul","Aug","Sep","Oct","Nov","Dec"
};

const char *level_to_string(log_level_t level){
    static const char *names[] = { "DEBUG","INFO","WARN","ERROR","FATAL" };
    return names[level];
}

static inline void format_file_component(const char *path, char *out, size_t out_size)
{
    const char *file = strrchr(path, '/');
    if (file)
        file++; 
    else
        file = path;

    size_t len = 0;
    const char *dot = strrchr(file, '.');
    if (dot)
        len = dot - file;
    else
        len = strlen(file);

    size_t i;
    for (i = 0; i < len && i < out_size - 1; i++) {
        char c = file[i];
        if (c >= 'a' && c <= 'z')
            c = c - 'a' + 'A';
        out[i] = c;
    }
    out[i] = '\0';
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
            snprintf(out, sizeof(out),
                "%s %02u %02u:%02u:%02u %-5s %s : %s",
                month_names[now.month - 1], now.day, now.hour, now.min, now.sec,
                level_to_string(level), component, msg);
        else
            snprintf(out, sizeof(out),
                "%s %02u %02u:%02u:%02u %-5s %s",
                month_names[now.month - 1], now.day, now.hour, now.min, now.sec,
                level_to_string(level), msg);
    } 
    else 
        snprintf(out, sizeof(out), "%s", msg);

    spinlock_acquire(&slock);
    arch_serial_write(out); arch_serial_write("\n");
    console_write(out);
    spinlock_release(&slock);
}

void _log(log_level_t level, const char *component, const char *file, const char *format, ...)
{
    va_list vargs;
    va_start(vargs, format);

    char comp_name[64];
    if (component)
        snprintf(comp_name, sizeof(comp_name), "%s", component);
    else
        format_file_component(file, comp_name, sizeof(comp_name));

    vlog(level, comp_name, format, vargs);

    va_end(vargs);
}
