#pragma once

#include "dev/acpi/acpi.h"

typedef struct
{
    uint8_t  address_space_id;
    uint8_t  register_bit_width;
    uint8_t  register_bit_offset;
    uint8_t  reserved;
    uint64_t address;
}
__attribute__((packed))
acpi_hpet_address_t;

typedef struct
{
    acpi_sdt_t          sdt;
    uint8_t             hardware_rev_id;
    uint8_t             comparator_count : 5;
    uint8_t             counter_size : 1;
    uint8_t             reserved : 1;
    uint8_t             legacy_replacement : 1;
    uint16_t            pci_vendor_id;
    acpi_hpet_address_t address;
    uint8_t             hpet_number;
    uint16_t            minimum_tick;
    uint8_t             page_protection;
}
__attribute__((packed))
acpi_hpet_table_t;
