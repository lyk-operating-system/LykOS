#include "arch/clock.h"

#include "arch/x86_64/ioport.h"
#include <stdint.h>

#define CMOS_ADDR   0x70
#define CMOS_DATA   0x71

#define RTC_REG_SECONDS     0x00
#define RTC_REG_MINUTES     0x02
#define RTC_REG_HOURS       0x04
#define RTC_REG_DAY         0x07
#define RTC_REG_MONTH       0x08
#define RTC_REG_YEAR        0x09
#define RTC_REG_STATUS_A    0x0A
#define RTC_REG_STATUS_B    0x0B

// Helpers

static bool is_updating()
{
      x86_64_ioport_outb(CMOS_ADDR, RTC_REG_STATUS_A);
      return (bool)(x86_64_ioport_inb(CMOS_DATA) & 0x80);
}

static uint8_t bcd_to_bin(uint8_t v)
{
    return (v & 0x0F) + ((v >> 4) * 10);
}

static uint8_t cmos_read(uint8_t reg)
{
    x86_64_ioport_outb(CMOS_ADDR, reg);
    return x86_64_ioport_inb(CMOS_DATA);
}

static void read_snapshot(arch_clock_snapshot_t *s)
{
    *s = (arch_clock_snapshot_t) {
        .sec   = cmos_read(RTC_REG_SECONDS),
        .min   = cmos_read(RTC_REG_MINUTES),
        .hour  = cmos_read(RTC_REG_HOURS),
        .day   = cmos_read(RTC_REG_DAY),
        .month = cmos_read(RTC_REG_MONTH),
        .year  = cmos_read(RTC_REG_YEAR)
    };
}

// API

bool arch_clock_get_snapshot(arch_clock_snapshot_t *out)
{
    arch_clock_snapshot_t a, b;

    // Wait until not updating.
    while (is_updating())
        ;

    read_snapshot(&a);

    uint8_t reg_b = cmos_read(RTC_REG_STATUS_B);

    // Read until stable (same values twice).
    while (true)
    {
        while (is_updating())
            ;

        read_snapshot(&b);

        if (a.sec == b.sec &&
            a.min == b.min &&
            a.hour == b.hour &&
            a.day == b.day &&
            a.month == b.month &&
            a.year == b.year)
            break;

        a = b;
    }
    *out = b;

    // Convert from BCD if needed.
    if (!(reg_b & 0x04)) // (1=binary, 0=BCD)
    {
        out->sec   = bcd_to_bin(out->sec);
        out->min   = bcd_to_bin(out->min);
        out->hour  = bcd_to_bin(out->hour & 0x7F);
        out->day   = bcd_to_bin(out->day);
        out->month = bcd_to_bin(out->month);
        out->year  = bcd_to_bin(out->year);
    }

    out->year += 2000;

    if (out->sec >= 60 || out->min >= 60 || out->hour >= 24 ||
        out->day < 1 || out->day > 31 ||
        out->month < 1 || out->month > 12 ||
        out->year < 1970)
        return false;
    
    return true;
}

uint64_t arch_clock_get_unix_time()
{
    arch_clock_snapshot_t now;
    arch_clock_get_snapshot(&now);

    now.year -= now.month <= 2;
    const int64_t era = (now.year >= 0 ? now.year : now.year - 399) / 400;
    const unsigned yoe = (unsigned)(now.year - era * 400);
    const unsigned doy = (153 * (now.month + (now.month > 2 ? -3 : 9)) + 2) / 5 + now.day - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    int64_t days = era * 146097 + (int64_t)doe - 719468;
    int64_t sod = now.hour * 3600 + now.min * 60 + now.sec;

    return (uint64_t)(days * 86400 + sod);
}
