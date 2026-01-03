#include "log.h"
#include "mod/module.h"

#include "dev/acpi/acpi.h"
#include "dev/acpi/tables/mcfg.h"
#include "dev/bus/pci.h"
#include "dev/device.h"
#include "hhdm.h"
#define LOG_PREFIX "PCI"
#include "log.h"
#include "utils/printf.h"

static device_class_t pci_class_to_device_class(uint8_t class)
{
    switch (class)
    {
        case 0x01: // Mass Storage
            return DEVICE_CLASS_BLOCK;
        case 0x02: // Network Controller
            return DEVICE_CLASS_NETWORK;
        case 0x09: // Input Device
            return DEVICE_CLASS_INPUT;
        default:
            return DEVICE_CLASS_UNKNOWN;
    }
}

void __module_install()
{
    acpi_mcfg_t *mcfg = (acpi_mcfg_t*)acpi_lookup("MCFG");
    if (!mcfg)
    {
        log(LOG_ERROR, "Could not find the MCFG table!");
        return;
    }

    bus_t *bus_pci = bus_register("pci", NULL, NULL);
    if (!bus_pci)
    {
        log(LOG_ERROR, "Could not register the PCI bus!");
        return;
    }

    for (uint64_t i = 0; i < (mcfg->sdt.length - sizeof(acpi_mcfg_t)) / 16; i++)
    {
        uint64_t base      = mcfg->segments[i].base_addr;
        uint64_t bus_start = mcfg->segments[i].bus_start;
        uint64_t bus_end   = mcfg->segments[i].bus_end;

        for (uint64_t bus = bus_start; bus <= bus_end; bus++)
        for (uint64_t dev = 0; dev < 32; dev++)
        for (uint64_t func = 0; func < 8; func++)
        {
            uintptr_t addr = HHDM + base + ((bus << 20) | (dev << 15) | (func << 12));
            pci_header_common_t *pci_hdr = (pci_header_common_t *) addr;
            if (pci_hdr->vendor_id == 0xFFFF)
                continue;

            char name[64];
            snprintf(
                name, sizeof(name),
                "%04X:%04X-%02X:%02X:%02X",
                pci_hdr->vendor_id, pci_hdr->device_id,
                pci_hdr->class, pci_hdr->subclass, pci_hdr->prog_if
            );

            device_t *dev = device_register(
                bus_pci,
                name,
                pci_class_to_device_class(pci_hdr->class),
                pci_hdr
            );
            if (dev)
                log(LOG_DEBUG, "Registered device: %s", name);
        }
    }

    log(LOG_INFO, "Successfully listed devices.");
}

MODULE_NAME("PCI")
MODULE_VERSION("0.1.0")
MODULE_DESCRIPTION("PCI bus enumeration and probing.")
MODULE_AUTHOR("Matei Lupu")
