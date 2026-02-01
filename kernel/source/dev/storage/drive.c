#include "dev/storage/drive.h"

#include "dev/bus.h"
#include "log.h"
#include "mm/heap.h"
#include "mm/mm.h"
#include "sync/spinlock.h"
#include "utils/list.h"
#include "utils/ref.h"

#define MAX_DRIVES 64

static list_t drive_list = LIST_INIT;
static spinlock_t drive_list_lock = SPINLOCK_INIT;
static int next_drive_id = 0;

drive_t *drive_create(drive_type_t type)
{
    drive_t *d = heap_alloc(sizeof(drive_t));
    if (!d)
        return NULL;

    memset(d, 0, sizeof(drive_t));

    d->type = type;
    d->device.class = DEVICE_STORAGE;

    ref_init(&d->device.refcount);
    d->device.slock = SPINLOCK_INIT;
    d->mounted = false;

    return d;
}

void drive_free(drive_t *d)
{
    if (!d || d->mounted)
        return;

    /* must not be mounted */
    heap_free(d);
}

void drive_mount(drive_t *d)
{
    bool do_bus_register = false;

    spinlock_acquire(&drive_list_lock);
    spinlock_acquire(&d->device.slock);

    if (!d->mounted && drive_list.length < MAX_DRIVES)
    {
        d->id = next_drive_id++;
        list_append(&drive_list, &d->node);
        d->mounted = true;

        ref_get(&d->device.refcount);
        do_bus_register = (d->device.bus != NULL);
    }
    else if (drive_list.length >= MAX_DRIVES)
    {
        log(LOG_ERROR, "Max drive number reached");
    }

    spinlock_release(&d->device.slock);
    spinlock_release(&drive_list_lock);

    if (do_bus_register)
        bus_register(d->device.bus);
}

void drive_unmount(drive_t *d)
{
    bool do_bus_remove = false;

    spinlock_acquire(&drive_list_lock);
    spinlock_acquire(&d->device.slock);

    if (d->mounted)
    {
        list_remove(&drive_list, &d->node);
        d->mounted = false;

        ref_put(&d->device.refcount);
        do_bus_remove = (d->device.bus != NULL);
    }

    spinlock_release(&d->device.slock);
    spinlock_release(&drive_list_lock);

    if (do_bus_remove)
        d->device.bus->remove_device(&d->device);
}

drive_t *drive_get(int id)
{
    drive_t *gd = NULL;

    if (id < 0)
        return NULL;

    spinlock_acquire(&drive_list_lock);

    FOREACH (n, drive_list)
    {
        drive_t *d = LIST_GET_CONTAINER(n, drive_t, node);
        if (d->id == id)
        {
            ref_get(&d->device.refcount);
            gd = d;
            break;
        }
    }

    spinlock_release(&drive_list_lock);
    return gd;
}

int drive_count(void)
{
    int count;

    spinlock_acquire(&drive_list_lock);
    count = drive_list.length;
    spinlock_release(&drive_list_lock);

    return count;
}
