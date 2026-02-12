#pragma once

#include <stddef.h>

void arch_lcpu_halt();

void arch_lcpu_int_mask();
void arch_lcpu_int_unmask();
[[nodiscard]] bool arch_lcpu_int_enabled();

void arch_lcpu_relax();

[[nodiscard]] size_t arch_lcpu_thread_reg_read();
void arch_lcpu_thread_reg_write(size_t t);

void arch_lcpu_init();
