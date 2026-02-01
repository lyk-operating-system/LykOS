#pragma once

#include <stdint.h>

void x86_64_ioapic_init();

void x86_64_ioapic_map_gsi(uint8_t gsi, uint8_t lapic_id, bool low_polarity, bool trigger_mode, uint8_t vector);

void x86_64_ioapic_map_legacy_irq(uint8_t irq, uint8_t lapic_id, bool fallback_low_polarity, bool fallback_trigger_mode, uint8_t vector);

int x86_64_ioapic_allocate_gsi();

void x86_64_ioapic_free_gsi(uint32_t gsi);
