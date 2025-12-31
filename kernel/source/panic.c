#include "panic.h"

#include "arch/lcpu.h"
#include "log.h"

void panic(const char *format, ...)
{
    arch_lcpu_int_mask();

    log(LOG_FATAL, "PANIC");
    va_list vargs;
    va_start(vargs);
    vlog(LOG_FATAL, NULL, format, vargs);
    va_end(vargs);

    while (true)
        arch_lcpu_halt();
}
