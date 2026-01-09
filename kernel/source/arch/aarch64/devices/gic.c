// API
#include "arch/aarch64/devices/gic.h"
//
#include "dev/acpi/tables/madt.h"
#include "hhdm.h"
#include "log.h"
#include "panic.h"

aarch64_gic_t *aarch64_gic;

extern aarch64_gic_t aarch64_gicv2;

void aarch64_gic_detect()
{
    acpi_madt_t *madt = (acpi_madt_t*)acpi_lookup("APIC");
    if (!madt)
        panic("MADT not found!");

    uint8_t *ptr = (uint8_t *)madt + sizeof(acpi_madt_t);
    uint8_t *end = (uint8_t *)madt + madt->sdt.length;

    uintptr_t gicc_base = 0;
    uintptr_t gicd_base = 0;

    while (ptr < end)
    {
        uint8_t type   = ptr[0];
        uint8_t length = ptr[1];

        switch (type)
        {
            case ACPI_MADT_TYPE_GICC:
            {
                acpi_madt_gicc_t *e = (acpi_madt_gicc_t*)ptr;
                gicc_base = e->phys_base_addr;
                break;
            }
            case ACPI_MADT_TYPE_GICD:
            {
                acpi_madt_gicd_t *e = (acpi_madt_gicd_t*)ptr;
                gicd_base = e->phys_base_addr;
                break;
            }
            default:
                break;
        }

        ptr += length;
    }

    log(LOG_DEBUG, "GICC base=%p GICD base=%p", gicc_base, gicd_base);
    if (!gicc_base || !gicd_base)
        panic("No GICC or GICD found in MADT");

    aarch64_gic = &aarch64_gicv2;
    aarch64_gic->set_base(gicc_base + HHDM, gicd_base + HHDM);
}
