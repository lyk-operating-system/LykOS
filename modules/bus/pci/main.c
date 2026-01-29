#include "mod/module.h"

#include "dev/acpi/acpi.h"
#include "dev/acpi/tables/mcfg.h"
#include "dev/bus.h"
#include "dev/bus/pci.h"
#include "dev/device.h"
#include "dev/driver.h"
#include "hhdm.h"
#include "mm/heap.h"
#define LOG_PREFIX "PCI"
#include "log.h"
#include "utils/printf.h"

static bool register_driver(driver_t *drv);

static bus_t pci_bus = {
    .name = "pci",
    .bridge = NULL,
    .devices = LIST_INIT,
    .drivers = LIST_INIT,
    .register_driver = register_driver
};

static bool register_driver(driver_t *drv)
{
    spinlock_acquire(&pci_bus.slock);

    if (drv->probe)
    {
        FOREACH (n, pci_bus.devices)
        {
            device_t *dev = LIST_GET_CONTAINER(n, device_t, list_node);

            if (!dev->driver)
                drv->probe(dev);
        }
    }

    spinlock_release(&pci_bus.slock);
    return true;
}

// Helper

static void pci_create_device(pci_header_common_t *pci_hdr)
{
    char *name = heap_alloc(32);
    snprintf(
        name, 32,
        "%04X:%04X-%02X:%02X:%02X",
        pci_hdr->vendor_id, pci_hdr->device_id,
        pci_hdr->class, pci_hdr->subclass, pci_hdr->prog_if
    );

    device_t *dev = heap_alloc(sizeof(device_t));
    *dev = (device_t) {
        .name = name,
        .bus = &pci_bus,
        .bus_data = pci_hdr,
    };
    ref_init(&dev->refcount);
    list_append(&pci_bus.devices, &dev->list_node);

    log(LOG_DEBUG, "Registered device: %s", name);
}

//

void __module_install()
{
    acpi_mcfg_t *mcfg = (acpi_mcfg_t*)acpi_lookup("MCFG");
    if (!mcfg)
    {
        log(LOG_ERROR, "Could not find the MCFG table!");
        return;
    }

    if (!bus_register(&pci_bus))
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

            pci_create_device(pci_hdr);
        }
    }

    log(LOG_INFO, "Successfully listed devices.");
}

void __module_destroy()
{
    log(LOG_INFO, "Module destroyed.");
}

MODULE_NAME("PCI")
MODULE_VERSION("0.1.0")
MODULE_DESCRIPTION("PCI bus enumeration.")
MODULE_AUTHOR("Matei Lupu")
