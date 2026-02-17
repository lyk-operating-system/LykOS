#define LOG_PREFIX "NVME"
#include "nvme.h"

#include "mod/module.h"
#include "log.h"
#include "dev/bus.h"
#include "dev/driver.h"

static int nvme_probe(device_t *device)
{
    volatile pci_header_type0_t *header = (volatile pci_header_type0_t *) device->bus_data;

    log(LOG_DEBUG, "Probing device: class=%02X subclass=%02X",
        header->common.class, header->common.subclass);

    if (header->common.class != 0x01 || header->common.subclass != 0x08)
        return 0;

    log(LOG_INFO, "Found NVMe controller!");
    nvme_init(header);
    return 1;
}

static driver_t nvme_driver = {
    .name = "NVMe Driver",
    .probe = nvme_probe,
};

void __module_install()
{
    bus_t *bus = bus_get("pci");
    if (!bus)
    {
        log(LOG_ERROR, "No PCI bus");
        return;
    }

    if (bus->register_driver(&nvme_driver))
        log(LOG_INFO, "Driver registered successfully.");
    else
        log(LOG_ERROR, "Error registering driver");

    bus_put(bus);
}

void __module_destroy()
{
    bus_t *bus = bus_get("pci");
    if (bus)
    {
        bus->remove_driver(&nvme_driver);
        bus_put(bus);
    }
}

MODULE_NAME("NVMe")
MODULE_VERSION("0.1.0")
MODULE_DESCRIPTION("Basic NVMe read & write");
MODULE_AUTHOR("Diana Petroșel")
