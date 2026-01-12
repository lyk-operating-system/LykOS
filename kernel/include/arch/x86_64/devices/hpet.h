#pragma once

#include <stdint.h>

uint64_t x86_64_hpet_get_frequency();
uint64_t x86_64_hpet_read_counter();
void x86_64_hpet_sleep_ns(uint64_t nanoseconds);

bool x86_64_hpet_init();
