#pragma once

#include <stddef.h>

extern size_t x86_64_fpu_area_size;
extern void (*x86_64_fpu_save)(void *area);
extern void (*x86_64_fpu_restore)(void *area);

void x86_64_fpu_init();

void x86_64_fpu_init_cpu();
