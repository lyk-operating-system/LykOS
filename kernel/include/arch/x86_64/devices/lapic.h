#pragma once

#include <stdint.h>

void x86_64_lapic_send_eoi();

void x86_64_lapic_ipi(uint32_t lapic_id, uint32_t vec);

void x86_64_lapic_init_cpu();
