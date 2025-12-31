#include "panic.h"
#include "log.h"
#include "arch/lcpu.h"

void panic(const char *format, ...)
{
    arch_lcpu_int_mask();

    log(LOG_FATAL, "PANIC");

    va_list vargs;
    va_start(vargs, format);
    vlog(LOG_FATAL, NULL, format, vargs);
    va_end(vargs);

    while (true)
        arch_lcpu_halt();
}
