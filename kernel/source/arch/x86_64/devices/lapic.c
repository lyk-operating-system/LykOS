// API
#include "arch/timer.h"
#include "arch/x86_64/devices/lapic.h"
//
#include "arch/irq.h"
#include "arch/x86_64/devices/pit.h"
#include "arch/x86_64/msr.h"
#include "hhdm.h"
#include "log.h"
#include "panic.h"

#include <stddef.h>

#define REG_ID 0x20
#define REG_SPURIOUS 0xF0
#define REG_EOI 0xB0
#define REG_IN_SERVICE_BASE 0x100
#define REG_ICR0 0x300
#define REG_ICR1 0x310
#define REG_TIMER_LVT 0x320
#define REG_TIMER_DIV 0x3E0
#define REG_TIMER_INITIAL_COUNT 0x380
#define REG_TIMER_CURRENT_COUNT 0x390

#define ONE_SHOOT    (0 << 17)
#define PERIODIC     (1 << 17)
#define TSC_DEADLINE (2 << 17)
#define MASK         (1 << 16)

#define IRQ 64

static uint64_t g_lapic_base;
static uint64_t g_lapic_timer_freq = 0;

static inline void lapic_write(uint32_t reg, uint32_t data)
{
    *(volatile uint32_t *)(g_lapic_base + reg) = data;
}

static inline uint32_t lapic_read(uint32_t reg)
{
    return *(volatile uint32_t *)(g_lapic_base + reg);
}

// timer.h API

void arch_timer_stop()
{
    lapic_write(REG_TIMER_LVT, MASK);
    lapic_write(REG_TIMER_INITIAL_COUNT, 0);
}

void arch_timer_oneshot(size_t us)
{
    lapic_write(REG_TIMER_LVT, MASK);
    lapic_write(REG_TIMER_LVT, ONE_SHOOT | (32 + IRQ));
    lapic_write(REG_TIMER_DIV, 0);
    lapic_write(REG_TIMER_INITIAL_COUNT, us * g_lapic_timer_freq / 1'000'000);
}

size_t arch_timer_get_local_irq()
{
    return IRQ;
}

// lapic.h

void x86_64_lapic_send_eoi()
{
    lapic_write(REG_EOI, 0);
}

void x86_64_lapic_ipi(uint32_t lapic_id, uint32_t vec)
{
    lapic_write(REG_ICR1, lapic_id << 24);
    lapic_write(REG_ICR0, vec);
}

// Initialization

void x86_64_lapic_init_cpu()
{
    g_lapic_base = (x86_64_msr_read(X86_64_MSR_APIC_BASE) & 0xFFFFFFFFFF000) + HHDM;
    lapic_write(REG_SPURIOUS, (1 << 8) | 0xFF);

    if (!g_lapic_timer_freq)
    {
        lapic_write(REG_TIMER_DIV, 0);
        for (uint64_t lapic_ticks = 8; /* :) */; lapic_ticks *= 2)
        {
            x86_64_pit_set_reload(UINT16_MAX);
            uint16_t pit_start = x86_64_pit_count();

            // Start LAPIC timer with `lapic_ticks` and wait for it to count down to 0.
            lapic_write(REG_TIMER_LVT, MASK);
            lapic_write(REG_TIMER_INITIAL_COUNT, lapic_ticks);
            while(lapic_read(REG_TIMER_CURRENT_COUNT) != 0)
                ;
            lapic_write(REG_TIMER_LVT, MASK);

            uint16_t pit_end = x86_64_pit_count();
            // Compute how many PIT ticks elapsed during the LAPIC countdown.
            uint16_t pit_delta = pit_start - pit_end;

            /*
             * For optimal calibration, measurements must be performed over a maximal time interval.
             * UINT16_MAX / 2 is not chosen as the threshold because if pit_delta were close to it the
             * next iteration might cause the counter to overflow, resulting in incorrect calibration.
             * Using a safer threshold of UINT16_MAX / 4 avoids this risk entirely.
             */
            if (pit_delta < UINT16_MAX / 4)
                continue;

            // Compute LAPIC timer frequency. Multiplications go first to avoid truncation.
            g_lapic_timer_freq = lapic_ticks * X86_64_PIT_BASE_FREQ / pit_delta;
            log(LOG_DEBUG, "LAPIC timer calibrated. Timer freq: %uHz", g_lapic_timer_freq);
            break;
        }
    }

    log(LOG_INFO, "LAPIC initialized.");
}
