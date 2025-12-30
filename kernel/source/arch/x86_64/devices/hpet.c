#include "arch/timer.h"
#include "hhdm.h"
#include "include/arch/x86_64/devices/hpet.h"
#include "include/dev/acpi/acpi.h"
#include "log.h"
#include "mm/mm.h"
#include "uapi/errno.h"

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

bool hpet_init()
{
    hpet_table_t *hpet_table = (hpet_table_t *)acpi_lookup("HPET");
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

uint64_t hpet_get_frequency()
{
    if (hpet_period_fs == 0)
        return 0;
    
    return 1000000000000000ULL / hpet_period_fs;
}

uint64_t hpet_read_counter()
{
    if (!hpet_base)
        return 0;
    
    return hpet_read_reg(HPET_MAIN_COUNTER_VALUE);
}

void hpet_sleep_ns(uint64_t nanoseconds)
{
    if (!hpet_base || hpet_period_fs == 0)
        return;

    uint64_t ticks = (nanoseconds * 1000000ULL) / hpet_period_fs;
    
    uint64_t start = hpet_read_counter();
    uint64_t end = start + ticks;

    // If overflow (for 32bit)
    if (end < start)
    {
        while (hpet_read_counter() > start)
            __asm__ volatile ("pause");
    }

    while (hpet_read_counter() < end)
        __asm__ volatile ("pause");
}

uint64_t arch_timer_get_uptime_ns(void)
{
    uint64_t cnt = hpet_read_counter();
    uint64_t freq = hpet_get_frequency();

    return (cnt * 1000000000ULL) / freq;
}