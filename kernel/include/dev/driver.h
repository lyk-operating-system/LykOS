#pragma once

#include "sync/spinlock.h"
#include "utils/list.h"
#include "utils/ref.h"

struct device;

typedef struct driver
{
    const char *name;
    struct bus_type *bus; // The bus this device is attached to
    list_t devices; // The devices managed by this driver

    int (*probe)(struct device *dev);
    int (*remove)(struct device *dev);
    void (*shutdown)(struct device *dev);

    list_node_t list_node; // Used inside bus_t
    ref_t refcount;
    spinlock_t slock;
}
driver_t;

bool driver_register(driver_t *driver);
