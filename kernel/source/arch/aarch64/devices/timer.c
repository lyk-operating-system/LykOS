// API
#include "arch/timer.h"
//
#include <stdint.h>

// Helpers

static inline uint64_t arch_timer_get_cntfrq()
{
    uint64_t v;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(v));
    return v;
}

static inline uint64_t arch_timer_read_cntvct()
{
    uint64_t v;
    asm volatile("isb");
    asm volatile("mrs %0, cntvct_el0" : "=r"(v));
    return v;
}

static inline void arch_timer_write_cntv_cval(uint64_t v)
{
    asm volatile("msr cntv_cval_el0, %0" :: "r"(v));
    asm volatile("isb");
}

static inline void arch_timer_write_cntv_ctl(uint64_t v)
{
    asm volatile("msr cntv_ctl_el0, %0" :: "r"(v));
    asm volatile("isb");
}

// timer.h API

void arch_timer_stop(void)
{
    // ENABLE=0, IMASK=1
    arch_timer_write_cntv_ctl(2);
}

void arch_timer_oneshot(size_t us)
{
    arch_timer_stop();

    uint64_t freq = arch_timer_get_cntfrq();
    uint64_t ticks = freq * us / 1000000;
    if (us && ticks == 0)
        ticks = 1;

    uint64_t now = arch_timer_read_cntvct();
    arch_timer_write_cntv_cval(now + ticks);

    // ENABLE=1, IMASK=0
    arch_timer_write_cntv_ctl(1);
}

size_t arch_timer_get_local_irq(void)
{
    return 28;
}

uint64_t arch_timer_get_uptime_ns(void)
{
    uint64_t cnt = arch_timer_read_cntvct();
    uint64_t freq = arch_timer_get_cntfrq();

    return (cnt * 1000000000ULL) / freq;
}

// Initialization

void aarch64_timer_init()
{

}
