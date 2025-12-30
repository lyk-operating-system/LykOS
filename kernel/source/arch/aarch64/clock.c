#include "arch/clock.h"

#include <stddef.h>

bool arch_clock_get_snapshot(arch_clock_snapshot_t *out)
{
    (void)out;
}

uint64_t arch_clock_get_unix_time()
{
    return 0;
}
