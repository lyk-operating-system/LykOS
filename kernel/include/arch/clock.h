#pragma once

#include <stdint.h>

typedef struct
{
    uint8_t sec;
    uint8_t min;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint16_t year;
}
arch_clock_snapshot_t;

bool arch_clock_get_snapshot(arch_clock_snapshot_t *out);

uint64_t arch_clock_get_unix_time();
