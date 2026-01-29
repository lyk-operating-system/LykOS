#include "mod/module.h"

#include "log.h"
#include "dev/bus.h"
#include "dev/driver.h"

#define LOG_PREFIX "NVME"

static driver_t nvme_driver = {
    .name = "NVMe Driver",
    .probe = NULL,
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
        log(LOG_ERROR, "Error registering drive");

    bus_put(bus);
}

MODULE_NAME("NVMe")
MODULE_VERSION("0.1.0")
MODULE_DESCRIPTION("NVMe ops.")
MODULE_AUTHOR("Diana Petroșel")
