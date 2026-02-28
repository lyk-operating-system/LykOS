#pragma once

#include <stdint.h>

typedef struct
{
    uint32_t _rsv0;
    uint32_t rsp0_lower;
    uint32_t rsp0_upper;
    uint32_t rsp1_lower;
    uint32_t rsp1_upper;
    uint32_t rsp2_lower;
    uint32_t rsp2_upper;
    uint32_t _rsv1;
    uint32_t _rsv2;
    uint32_t ist1_lower;
    uint32_t ist1_upper;
    uint32_t ist2_lower;
    uint32_t ist2_upper;
    uint32_t ist3_lower;
    uint32_t ist3_upper;
    uint32_t ist4_lower;
    uint32_t ist4_upper;
    uint32_t ist5_lower;
    uint32_t ist5_upper;
    uint32_t ist6_lower;
    uint32_t ist6_upper;
    uint32_t ist7_lower;
    uint32_t ist7_upper;
    uint32_t _rsv3;
    uint32_t _rsv4;
    uint16_t _rsv5;
    uint16_t iomap_base;
}
__attribute__((packed))
x86_64_tss_t;

extern x86_64_tss_t x86_64_tss[32];

void x86_64_tss_set_rsp0(x86_64_tss_t *tss, uintptr_t stack_pointer);
