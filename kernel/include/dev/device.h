#pragma once

#include "sync/spinlock.h"
#include "utils/list.h"
#include "utils/ref.h"
#include <stdatomic.h>
#include <stdint.h>

struct device;
struct driver;
struct bus;

typedef enum
{
    DEVICE_DULL,    // Generic
    DEVICE_AUDIO,
    DEVICE_BUS,     // Bus or controller devices eg. AHCI
    DEVICE_DISPLAY,
    DEVICE_NETWORK,
    DEVICE_STORAGE
}
device_class_t;

struct dev_pm_ops
{
    int (*suspend)(struct device *dev);
    int (*resume) (struct device *dev);
    int (*poweroff)(struct device *dev);
};

typedef struct device
{
    const char *name;
    struct device *parent; // Upstream bridge/controller
    struct driver *driver; // The driver bound to this device
    struct bus *bus;       // The bus this device is attached to

    device_class_t class;

    void *driver_data; // Private data for the loaded driver
    void *bus_data;    // Bus-specific identification data

    struct dev_pm_ops *power_ops;

    list_node_t list_node; // Used inside bus_t
    ref_t refcount;
    spinlock_t slock;
}
device_t;
