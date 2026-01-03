#pragma once

#include "dev/acpi/acpi.h"

typedef struct
{
    acpi_sdt_t sdt;
    uint64_t _rsv;
    struct
    {
        uint64_t base_addr;
        uint16_t segment_group;
        uint8_t  bus_start;
        uint8_t  bus_end;
        uint32_t _rsv;
    }
    __attribute__((packed))
    segments[];
}
__attribute__((packed))
acpi_mcfg_t;
