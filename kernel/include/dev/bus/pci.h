#pragma once

#include <stdint.h>

typedef struct
{
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t command;
    uint16_t status;
    uint8_t  revision_id;
    uint8_t  prog_if;
    uint8_t  subclass;
    uint8_t  class;
    uint8_t  cache_line_size;
    uint8_t  latency_timer;
    uint8_t  header_type;
    uint8_t  bist;
}
__attribute__((packed))
pci_header_common_t;

typedef struct
{
    pci_header_common_t common;
    uint32_t bar[6];
    uint32_t cardbus_cis_ptr;
    uint16_t subsystem_vendor_id;
    uint16_t subsystem_id;
    uint32_t expansion_rom_base;
    uint8_t  capabilities_ptr;
    uint8_t  _rsv[7];
    uint8_t  interrupt_line;
    uint8_t  interrupt_pin;
    uint8_t  min_grant;
    uint8_t  max_latency;
}
__attribute__((packed))
pci_header_type0_t;

typedef struct
{
    pci_header_common_t common;
    uint32_t bar[2];
    uint8_t  primary_bus;
    uint8_t  secondary_bus;
    uint8_t  subordinate_bus;
    uint8_t  secondary_latency_timer;
    uint8_t  io_base;
    uint8_t  io_limit;
    uint16_t secondary_status;
    uint16_t memory_base;
    uint16_t memory_limit;
    uint16_t prefetchable_memory_base;
    uint16_t prefetchable_memory_limit;
    uint32_t prefetchable_base_upper32;
    uint32_t prefetchable_limit_upper32;
    uint16_t io_base_upper16;
    uint16_t io_limit_upper16;
    uint8_t  capabilities_ptr;
    uint8_t  reserved1[3];
    uint32_t expansion_rom_base;
    uint8_t  interrupt_line;
    uint8_t  interrupt_pin;
    uint16_t bridge_control;
}
__attribute__((packed))
pci_header_type1_t;

typedef struct
{
    pci_header_common_t common;
    uint32_t bar[1];
    uint8_t  capabilities_ptr;
    uint8_t  _rsv1[3];
    uint32_t expansion_rom_base;
    uint8_t  interrupt_line;
    uint8_t  interrupt_pin;
    uint8_t  _rsv2[2];
    uint32_t socket_base;
    uint8_t  offset_capabilities;
    uint8_t  _rsv3[3];
    uint32_t legacy_mode_base;
}
__attribute__((packed))
pci_header_type2_t;

void pci_init();
