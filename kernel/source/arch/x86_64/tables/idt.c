// API
#include "arch/x86_64/tables/idt.h"
//
#include "arch/lcpu.h"
#include "arch/x86_64/tables/gdt.h"
#include "log.h"
#include <stddef.h>
#include <stdint.h>

typedef struct
{
    uint16_t isr_low;
    uint16_t kernel_cs;
    uint8_t  ist;
    uint8_t  flags;
    uint16_t isr_mid;
    uint32_t isr_high;
    uint32_t _rsv;
}
__attribute__((packed))
idt_entry_t;

typedef struct
{
    uint16_t limit;
    uint64_t base;
}
__attribute__((packed))
idtr_t;

__attribute__((aligned(0x10))) static idt_entry_t idt[256];
extern uintptr_t __int_stub_table[256];

void x86_64_idt_init()
{
    for (int i = 0; i < 256; i++)
    {
        uint64_t isr = (uint64_t)__int_stub_table[i];

        idt[i] = (idt_entry_t) {
            .isr_low = isr & 0xFFFF,
            .kernel_cs = GDT_SELECTOR_CODE64_RING0,
            .ist = 0,
            .flags = 0x8E,
            .isr_mid = (isr >> 16) & 0xFFFF,
            .isr_high = (isr >> 32) & 0xFFFFFFFF,
            ._rsv = 0
        };
    }

    log(LOG_INFO, "IDT generated.");
}

void x86_64_idt_init_cpu()
{
    idtr_t idtr = (idtr_t) {
        .limit = sizeof(idt) - 1,
        .base = (uint64_t)&idt
    };
    asm volatile("lidt %0" : : "m"(idtr));

    arch_lcpu_int_unmask();

    log(LOG_INFO, "IDT loaded");
}
