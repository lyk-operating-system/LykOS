#pragma once

#include "utils/list.h"
#include "sync/spinlock.h"
#include <stdint.h>
#include <stddef.h>

typedef enum
{
    DEVICE_CLASS_UNKNOWN,
    DEVICE_CLASS_BLOCK,
    DEVICE_CLASS_INPUT,
    DEVICE_CLASS_NETWORK
}
device_class_t;

typedef enum
{
    RESOURCE_DMA,
    RESOURCE_IRQ,
    RESOURCE_MMIO
}
resource_type_t;

typedef struct
{
    resource_type_t type;
    size_t start;
    size_t size;
    void *private;

    list_node_t list_node; // used inside device_t
}
resource_t;

typedef struct bus bus_t;
typedef struct device device_t;
typedef struct driver driver_t;

struct bus
{
    const char *name;

    list_t devices;
    list_t drivers;

    int  (*probe)(device_t *dev);
    void (*remove)(device_t *dev);

    list_node_t list_node;
    spinlock_t slock;
};

typedef struct block_device_ops block_device_ops_t;
typedef struct input_device_ops input_device_ops_t;
typedef struct network_device_ops network_device_ops_t;

struct device
{
    const char *name;
    device_class_t class;
    bus_t *bus;
    driver_t *driver;

    void *device_ident_data;

    list_t resources;
    void *driver_data;

    union
    {
        block_device_ops_t *block;
        network_device_ops_t *net;
    }
    ops;

    list_node_t list_node; // used inside bus_t
    spinlock_t slock;
};

struct driver
{
    const char *name;
    device_class_t class;
    bus_t *bus;

    bool (*probe)(device_t *dev);
    bool (*bind)(device_t *dev);
    void (*remove)(device_t *dev);

    void (*suspend)(device_t *dev);
    void (*resume)(device_t *dev);

    list_node_t list_node; // used inside bus_t
    spinlock_t slock;
};

bus_t *bus_register(
    const char *name,
    int  (*probe) (device_t *),
    void (*remove)(device_t *)
);
bus_t *bus_lookup(const char *name);

device_t *device_register(
    bus_t           *bus,
    const char      *name,
    device_class_t   class,
    void            *device_ident_data
);

driver_t *driver_register(
    bus_t           *bus,
    const char      *name,
    device_class_t   class,
    bool (*probe)   (device_t *dev),
    bool (*bind)    (device_t *dev),
    void (*remove)  (device_t *dev),
    void (*suspend) (device_t *dev),
    void (*resume)  (device_t *dev)
);
