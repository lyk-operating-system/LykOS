#pragma once

#include <stdint.h>

bool hpet_init();
uint64_t hpet_get_frequency();
uint64_t hpet_read_counter();
void hpet_sleep_ns(uint64_t nanoseconds);
