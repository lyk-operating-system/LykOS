#pragma once

#include "sync/spinlock.h"
#include "utils/list.h"
#include "utils/ref.h"

struct device;
struct driver;

typedef struct bus
{
    const char *name;
    // The bridge device providing this bus or NULL for root.
    struct device *bridge;
    list_t devices;
    list_t drivers;

    void *private_data; // Bus-specific context

    bool (*register_device)(struct device *dev);
    bool (*remove_device)(struct device *dev);
    bool (*register_driver)(struct device *drv);
    bool (*remove_driver)(struct driver *drv);
    bool (*match)(struct device *dev, struct driver *drv);

    list_node_t list_node; // Part of a global list of buses
    ref_t refcount;
    spinlock_t slock;
}
bus_t;

bus_t *bus_get(const char *name);
void bus_put(bus_t *bus);

bool bus_register(bus_t *bus);
