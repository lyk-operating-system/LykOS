#include "arch/x86_64/devices/hpet.h"
#include "arch/timer.h"
//
#include "hhdm.h"
#include "dev/acpi/acpi.h"
#include "dev/acpi/tables/hpet.h"
#include "log.h"
#include "mm/mm.h"
#include <stdint.h>

#define HPET_GENERAL_CAPABILITIES   0x00
#define HPET_GENERAL_CONFIG         0x10
#define HPET_GENERAL_INT_STATUS     0x20
#define HPET_MAIN_COUNTER_VALUE     0xF0

// (N = 0-31)
#define HPET_TIMER_CONFIG(n)        (0x100 + (n) * 0x20)
#define HPET_TIMER_COMPARATOR(n)    (0x108 + (n) * 0x20)

// General config
#define HPET_CONFIG_ENABLE          (1 << 0) // HPET on
#define HPET_CONFIG_LEGACY_RT       (1 << 1)

// Per timer
#define HPET_TIMER_INT_ENABLE       (1 << 2) // Timer for interrupts
#define HPET_TIMER_TYPE_PERIODIC    (1 << 3) // Periodic || one-shot
#define HPET_TIMER_CAP_PERIODIC     (1 << 4)
#define HPET_TIMER_CAP_64BIT        (1 << 5)
#define HPET_TIMER_SET_ACCUMULATOR  (1 << 6)
#define HPET_TIMER_FORCE_32BIT      (1 << 8)

static volatile void *hpet_base = NULL;
static uint64_t hpet_period_fs = 0;

static inline uint64_t hpet_read_reg(uint64_t offset)
{
    return *(volatile uint64_t *)((uintptr_t)hpet_base + offset);
}

static inline void hpet_write_reg(uint64_t offset, uint64_t value)
{
    *(volatile uint64_t *)((uintptr_t)hpet_base + offset) = value;
}

bool x86_64_hpet_init()
{
    acpi_hpet_table_t *hpet_table = (acpi_hpet_table_t *)acpi_lookup("HPET");
    if(!hpet_table)
        return false;

    hpet_base = (volatile void *)(uintptr_t)(hpet_table->address.address + HHDM);
    uint64_t capabilities = hpet_read_reg(HPET_GENERAL_CAPABILITIES);
    hpet_period_fs = capabilities >> 32;

    // Disable, set main counter to 0, enable
    uint64_t config = hpet_read_reg(HPET_GENERAL_CONFIG);
    config &= ~HPET_CONFIG_ENABLE;
    hpet_write_reg(HPET_GENERAL_CONFIG, config);
    hpet_write_reg(HPET_MAIN_COUNTER_VALUE, 0);
    config |= HPET_CONFIG_ENABLE;
    hpet_write_reg(HPET_GENERAL_CONFIG, config);

    log(LOG_DEBUG, "HPET initialized.");
    return true;
}

uint64_t x86_64_hpet_get_frequency()
{
    if (hpet_period_fs == 0)
        return 0;

    return 1'000'000'000'000'000ULL / hpet_period_fs;
}

uint64_t x86_64_hpet_read_counter()
{
    if (!hpet_base)
        return 0;

    return hpet_read_reg(HPET_MAIN_COUNTER_VALUE);
}

void x86_64_hpet_sleep_ns(uint64_t nanoseconds)
{
    if (!hpet_base || hpet_period_fs == 0)
        return;

    uint64_t ticks = (nanoseconds * 1'000'000ULL) / hpet_period_fs;

    uint64_t start = x86_64_hpet_read_counter();
    uint64_t end = start + ticks;

    // If overflow (for 32bit)
    if (end < start)
    {
        while (x86_64_hpet_read_counter() > start)
            asm volatile ("pause");
    }

    while (x86_64_hpet_read_counter() < end)
        asm volatile ("pause");
}

uint64_t arch_timer_get_uptime_ns()
{
    uint64_t cnt = x86_64_hpet_read_counter();
    uint64_t freq = x86_64_hpet_get_frequency();

    return (cnt * 1'000'000'000ULL) / freq;
}
