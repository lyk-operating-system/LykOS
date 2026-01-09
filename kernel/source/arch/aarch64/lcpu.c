// API
#include "arch/lcpu.h"
//
#include "arch/aarch64/devices/gic.h"
#include "arch/aarch64/devices/timer.h"
#include "arch/aarch64/int.h"

void arch_lcpu_halt()
{
    asm volatile("wfi");
}

void arch_lcpu_int_mask()
{
    asm volatile("msr daifset, #0b1111");
}

void arch_lcpu_int_unmask()
{
    asm volatile("msr daifclr, #0b1111");
}

bool arch_lcpu_int_enabled(void)
{
    unsigned long daif;
    asm volatile("mrs %0, daif" : "=r"(daif));
    return (daif & (1 << 7)) == 0;
}

void arch_lcpu_relax()
{
    asm volatile("yield");
}

size_t arch_lcpu_thread_reg_read()
{
    size_t ret;
    asm volatile("mrs %0, tpidr_el1" : "=r"(ret));
    return ret;
}

void arch_lcpu_thread_reg_write(size_t t)
{
    asm volatile("msr tpidr_el1, %0" : : "r"(t));
}

void arch_lcpu_init()
{
    aarch64_int_init_cpu();
    aarch64_gic->gicc_init();
    aarch64_timer_init_cpu();
}
