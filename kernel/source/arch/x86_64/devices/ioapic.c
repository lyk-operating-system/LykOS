#include "arch/x86_64/devices/ioapic.h"
//
#include "assert.h"
#include "dev/acpi/tables/madt.h"
#include "hhdm.h"
#include "log.h"
#include "panic.h"
#include <stddef.h>

#define IOREGSEL  0x00
#define IOWIN     0x10

#define IOAPICID  0x00
#define IOAPICVER 0x01
#define IOREDTBL(N) (0x10 + 2 * (N))

#define SO_FLAG_POLARITY_LOW  0b0011
#define SO_FLAG_POLARITY_HIGH 0b0001
#define SO_TRIGGER_EDGE       0b0100
#define SO_TRIGGER_LEVEL      0b1100

typedef struct
{
    uint64_t vector          :  8;
    uint64_t delivery_mode   :  3;
    uint64_t dest_mode       :  1;
    uint64_t delivery_status :  1;
    uint64_t pin_polarity    :  1;
    uint64_t remote_irr      :  1;
    uint64_t trigger_mode    :  1;
    uint64_t mask            :  1;
    uint64_t _rsv            : 39;
    uint64_t destination     :  8;
}
__attribute__((packed))
ioapic_redirect_t;
static_assert(sizeof(ioapic_redirect_t) == 8);

static struct
{
    uint16_t dest;
    uint16_t flags;
}
g_irq_redirection_table[16] = {
    {  0, 0 },
    {  1, 0 },
    {  2, 0 },
    {  3, 0 },
    {  4, 0 },
    {  5, 0 },
    {  6, 0 },
    {  7, 0 },
    {  8, 0 },
    {  9, 0 },
    { 10, 0 },
    { 11, 0 },
    { 12, 0 },
    { 13, 0 },
    { 14, 0 },
    { 15, 0 }
};

typedef struct
{
    uintptr_t base;
    uint32_t gsi_base;
    uint32_t gsi_count;
}
ioapic_t;
static ioapic_t ioapics[8];
static size_t   ioapic_count = 0;

/*
 * MMIO helpers
 */

static void ioapic_write(uintptr_t base, uint32_t reg, uint32_t data)
{
    *(volatile uint32_t *)(base + IOREGSEL) = reg; // IOREGSEL
    *(volatile uint32_t *)(base + IOWIN) = data;   // IOWIN
}

static uint32_t ioapic_read(uintptr_t base, uint32_t reg)
{
    *(volatile uint32_t *)(base + IOREGSEL) = reg; // IOREGSEL
    return *(volatile uint32_t *)(base + IOWIN);   // IOWIN
}

/*
 * GSI allocation
 */

#define MAX_GSIS 256
static uint8_t gsi_bitmap[MAX_GSIS / 8] = {0};

#define BITMAP_SET(bit) (gsi_bitmap[(bit) / 8] |= (1 << ((bit) % 8)))
#define BITMAP_CLEAR(bit) (gsi_bitmap[(bit) / 8] &= ~(1 << ((bit) % 8)))
#define BITMAP_TEST(bit) (gsi_bitmap[(bit) / 8] & (1 << ((bit) % 8)))

int x86_64_ioapic_allocate_gsi()
{
    for (size_t i = 0; i < MAX_GSIS; i++)
    {
        if (!BITMAP_TEST(i))
        {
            BITMAP_SET(i);
            return (int)i;
        }
    }

    return -1;
}

void x86_64_ioapic_free_gsi(uint32_t gsi)
{
    ASSERT(gsi < MAX_GSIS);

    BITMAP_CLEAR(gsi);
}

/*
 * Main IOAPIC code
 */

void x86_64_ioapic_init()
{
    acpi_madt_t *madt = (acpi_madt_t *)acpi_lookup("APIC");
    if (!madt)
        panic("MADT not found!");

    uint8_t *ptr = (uint8_t *)madt + sizeof(acpi_madt_t);
    uint8_t *end = (uint8_t *)madt + madt->sdt.length;
    while (ptr < end)
    {
        uint8_t type = ptr[0];
        uint8_t length = ptr[1];

        switch (type)
        {
            case ACPI_MADT_TYPE_IOAPIC:
            {
                acpi_madt_ioapic_t *madt_ioapic = (acpi_madt_ioapic_t *)ptr;

                uintptr_t base = madt_ioapic->ioapic_addr + HHDM;
                uint32_t gsi_base = madt_ioapic->gsi_base;
                size_t gsi_count = ((ioapic_read(base, IOAPICVER) >> 16) & 0xFF) + 1;

                ioapic_t *ioapic = &ioapics[ioapic_count++];
                *ioapic = (ioapic_t) {
                    .base = base,
                    .gsi_base = gsi_base,
                    .gsi_count = gsi_count
                };
                break;
            }
            case ACPI_MADT_TYPE_SOURCE_OVERRIDE:
            {
                acpi_madt_int_source_override_t *so = (acpi_madt_int_source_override_t *)ptr;
                g_irq_redirection_table[so->source].dest  = so->gsi;
                g_irq_redirection_table[so->source].flags = so->flags;
                BITMAP_SET(so->gsi); // Reserve this GSI for legacy hardware.
                break;
            }
        }

        ptr += length;
    }

    if (ioapic_count != 0)
        log(LOG_INFO, "IOAPIC initialized.");
    else
        panic("IOAPIC interrupt controller structure not found!");
}

static ioapic_t *find_ioapic(uint32_t gsi)
{
    for (size_t i = 0; i < ioapic_count; i++)
    {
        uint32_t max_gsi = ioapics[i].gsi_base + ioapics[i].gsi_count;
        if (gsi >= ioapics[i].gsi_base && gsi < max_gsi)
        {
            return &ioapics[i];
        }
    }

    return NULL;
}

void x86_64_ioapic_map_gsi(uint8_t gsi, uint8_t lapic_id, bool low_polarity, bool trigger_mode, uint8_t vector)
{
    ioapic_t *target = find_ioapic(gsi);
    if (!target)
        panic("No target IOAPIC found!");

    ioapic_redirect_t entry = {
        .vector          = vector,
        .delivery_mode   = 0,    // Fixed
        .dest_mode       = 0,    // Physical
        .delivery_status = 0,    // Read-only
        .pin_polarity    = low_polarity ? 1 : 0,
        .remote_irr      = 0,    // Read-only
        .trigger_mode    = trigger_mode ? 1 : 0,
        .mask            = 0,    // Unmasked
        ._rsv            = 0,
        .destination     = lapic_id
    };

    uint32_t low  = *(uint32_t *)&entry;
    uint32_t high = *((uint32_t *)&entry + 1);

    uint32_t index = gsi - target->gsi_base;
    ioapic_write(target->base, IOREDTBL(index), low);
    ioapic_write(target->base, IOREDTBL(index) + 1, high);
}

void x86_64_ioapic_map_legacy_irq(uint8_t irq, uint8_t lapic_id, bool fallback_low_polarity, bool fallback_trigger_mode, uint8_t vector)
{
    if(irq < 16)
    {
        // Polarity.
        switch(g_irq_redirection_table[irq].flags & 0b0011)
        {
            case SO_FLAG_POLARITY_LOW:
                fallback_low_polarity = true;
                break;
            case SO_FLAG_POLARITY_HIGH:
                fallback_low_polarity = false;
                break;
        }
        // Trigger mode.
        switch(g_irq_redirection_table[irq].flags & 0b1100)
        {
            case SO_TRIGGER_EDGE:
                fallback_trigger_mode = false;
                break;
            case SO_TRIGGER_LEVEL:
                fallback_trigger_mode = true;
                break;
        }

        irq = g_irq_redirection_table[irq].dest;
    }

    x86_64_ioapic_map_gsi(irq, lapic_id, fallback_low_polarity, fallback_trigger_mode, vector);
}
