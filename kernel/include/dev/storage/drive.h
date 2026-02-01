#pragma once

#include "dev/device.h"
#include "utils/list.h"

typedef enum
{
    DRIVE_TYPE_HDD,
    DRIVE_TYPE_SSD,
    DRIVE_TYPE_NVME,
    DRIVE_TYPE_USB,
    DRIVE_TYPE_NETWORK
}
drive_type_t;

typedef struct drive
{
    device_t device;
    int id;
    bool mounted;

    drive_type_t type;
    const char *serial;
    const char *model;
    const char *vendor;
    const char *revision;

    uint64_t sectors;
    uint64_t sector_size;

    // Function pointers for actual I/O — filled in by the driver
    int (*read_sectors)(struct drive *d, const void *buf, uint64_t lba, uint64_t count);
    int (*write_sectors)(struct drive *d, const void *buf, uint64_t lba, uint64_t count);

    list_node_t node;
}
drive_t;

drive_t *drive_create(drive_type_t type);
void drive_free(drive_t *d);

void drive_mount(drive_t *d);
drive_t *drive_get(int id);
int drive_counter(void);
