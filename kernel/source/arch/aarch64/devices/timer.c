// API
#include "arch/timer.h"
//
#include <arch/aarch64/devices/gic.h>
#include <stdint.h>

// Helpers

static inline uint64_t read_cntfrq()
{
    uint64_t v;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(v));
    return v;
}

static inline uint64_t read_cntvct()
{
    uint64_t v;
    asm volatile("mrs %0, cntvct_el0" : "=r"(v));
    return v;
}

static inline void write_cntv_ctl(uint64_t v)
{
    asm volatile("msr cntv_ctl_el0, %0" :: "r"(v));
    asm volatile("isb");
}

static inline void write_cntv_tval(uint64_t v)
{
    asm volatile("msr cntv_tval_el0, %0" :: "r"(v));
    asm volatile("isb");
}

// timer.h API

void arch_timer_stop()
{
    // ENABLE=0, IMASK=1
    write_cntv_ctl(2);
}

void arch_timer_oneshot(size_t us)
{
    arch_timer_stop();

    uint64_t freq = read_cntfrq();
    uint64_t ticks = (freq * us) / 1'000'000ull;
    if (us != 0 && ticks == 0)
        ticks = 1;

    write_cntv_tval(ticks);

    // ENABLE=1, IMASK=0
    write_cntv_ctl(1);
}

uint64_t arch_timer_get_uptime_ns()
{
    uint64_t cnt = read_cntvct();
    uint64_t freq = read_cntfrq();

    return (cnt * 1'000'000'000ull) / freq;
}

// Initialization

void aarch64_timer_init_cpu()
{
    // Enable read-only access to the virtual counter for EL0 without trapping to EL1.
    // This is important for performance reasons as it skips the need for a system call.
    uint64_t cntkctl;
    asm volatile("mrs %0, cntkctl_el1" : "=r"(cntkctl));
    cntkctl |= (1 << 1); // EL1VCTEN
    asm volatile("msr cntkctl_el1, %0" :: "r"(cntkctl));
    asm volatile("isb");

    arch_timer_stop();
    aarch64_gic->enable_int(27);
}
