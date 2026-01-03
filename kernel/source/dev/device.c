#include "dev/device.h"

#include "mm/heap.h"
#include "utils/string.h"

static list_t buses;
static spinlock_t slock;

bus_t *bus_register(
    const char *name,
    int  (*probe)(device_t *),
    void (*remove)(device_t *)
)
{
    spinlock_acquire(&slock);

    FOREACH (n, buses)
    {
         bus_t *bus = LIST_GET_CONTAINER(n, bus_t, list_node);
         if (strcmp(name, bus->name) == 0)
             return NULL;
    }

    bus_t *bus = heap_alloc(sizeof(bus_t));
    *bus = (bus_t) {
        .name = strdup(name),
        .devices = LIST_INIT,
        .drivers = LIST_INIT,
        .probe = probe,
        .remove = remove,
        .list_node = LIST_NODE_INIT,
        .slock = SPINLOCK_INIT,
    };
    list_append(&buses, &bus->list_node);

    spinlock_release(&slock);
    return bus;
}

bus_t *bus_lookup(const char *name)
{
    spinlock_acquire(&slock);

    FOREACH(n, buses)
    {
        bus_t *bus = LIST_GET_CONTAINER(n, bus_t, list_node);
        if (strcmp(name, bus->name) == 0)
        {
            spinlock_release(&slock);
            return bus;
        }
    }

    spinlock_release(&slock);
    return NULL;
}

device_t *device_register(
    bus_t           *bus,
    const char      *name,
    device_class_t   class,
    void            *device_ident_data
)
{
    if (!bus)
        return NULL;

    spinlock_acquire(&bus->slock);

    FOREACH (n, bus->devices)
    {
        device_t *dev = LIST_GET_CONTAINER(n, device_t, list_node);
        if (strcmp(name, dev->name) == 0)
        {
            spinlock_release(&bus->slock);
            return NULL;
        }
    }

    device_t *dev = heap_alloc(sizeof(device_t));
    *dev = (device_t) {
        .name = strdup(name),
        .class = class,
        .bus = bus,
        .driver = NULL,
        .device_ident_data = device_ident_data,
        .resources = LIST_INIT,
        .driver_data = NULL,
        .ops = {0},
        .list_node = LIST_NODE_INIT,
        .slock = SPINLOCK_INIT,
    };
    list_append(&bus->devices, &dev->list_node);

    spinlock_release(&bus->slock);
    return dev;
}

driver_t *driver_register(
    bus_t           *bus,
    const char      *name,
    device_class_t   class,
    bool (*probe)(device_t *),
    bool (*bind)(device_t *),
    void (*remove)(device_t *),
    void (*suspend)(device_t *),
    void (*resume)(device_t *)
)
{
    if (!bus)
        return NULL;

    spinlock_acquire(&bus->slock);

    FOREACH (n, bus->drivers)
    {
        driver_t *drv = LIST_GET_CONTAINER(n, driver_t, list_node);
        if (strcmp(name, drv->name) == 0)
        {
            spinlock_release(&bus->slock);
            return NULL;
        }
    }

    driver_t *drv = heap_alloc(sizeof(driver_t));
    *drv = (driver_t) {
        .name = strdup(name),
        .class = class,
        .bus = bus,
        .probe = probe,
        .bind = bind,
        .remove = remove,
        .suspend = suspend,
        .resume = resume,
        .list_node = LIST_NODE_INIT,
        .slock = SPINLOCK_INIT,
    };
    list_append(&bus->drivers, &drv->list_node);

    spinlock_release(&bus->slock);
    return drv;
}
