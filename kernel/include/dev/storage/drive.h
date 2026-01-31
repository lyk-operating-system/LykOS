#pragma once

#include "dev/bus.h"
#include "dev/device.h"
#include "log.h"
#include "mm/heap.h"
#include "mm/mm.h"
#include "sync/spinlock.h"
#include "utils/ref.h"

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

    drive_type_t type;
    const char *serial;
    const char *model;
    const char *vendor;
    const char *revision;

    uint64_t sectors;
    uint64_t sector_size;

    // function pointers for actual I/O - filled in by the driver
    int (*read_sectors)(struct drive *d, const void *buf, uint64_t lba, uint64_t count);
    int (*write_sectors)(struct drive *d, const void *buf, uint64_t lba, uint64_t count);

}
drive_t;

drive_t *drive_create(drive_type_t type);
void drive_free(drive_t *d);

void drive_mount(drive_t *d);
drive_t *drive_get(int id);
int drive_counter(void);
