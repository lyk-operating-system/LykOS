#include "assert.h"
#include "dev/bus.h"
#include "dev/device.h"

static bool virtual_bus_device_register(struct device *dev);

static bus_t virtual_bus = {
    .name = "virtual",
    .bridge = NULL,
    .devices = LIST_INIT,
    .drivers = LIST_INIT,
    .register_device = virtual_bus_device_register,
};

static bool virtual_bus_device_register(struct device *dev)
{
    spinlock_acquire(&virtual_bus.slock);

    dev->parent = NULL;
    dev->driver = NULL;
    ref_get(&virtual_bus.refcount);
    dev->bus = &virtual_bus;
    dev->list_node = LIST_NODE_INIT;
    ref_init(&dev->refcount);
    dev->slock = SPINLOCK_INIT;

    ref_get(&dev->refcount);
    list_append(&virtual_bus.devices, &dev->list_node);

    spinlock_release(&virtual_bus.slock);
    return true;
}

extern void virtual_fb_init();

void virtual_devices_init()
{
    bool ret = bus_register(&virtual_bus);
    ASSERT(ret);

    virtual_fb_init();
}
