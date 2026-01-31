#include "dev/storage/drive.h"
#include "sync/spinlock.h"
#include "utils/ref.h"

#define MAX_DRIVES 64

static drive_t *drives[MAX_DRIVES];
static int _drive_count = 0;
static spinlock_t drives_lock = SPINLOCK_INIT;

drive_t *drive_create(drive_type_t type)
{
    // kmem_cache_t *drive_cache = kmem_new_cache("drive_t", sizeof(drive_t));
    drive_t *d = heap_alloc(sizeof(drive_t));
    if (!d)
        return NULL;

    memset(d, 0, sizeof(drive_t));

    d->type = type;
    d->device.class = DEVICE_STORAGE;

    ref_init(&d->device.refcount);
    d->device.slock = SPINLOCK_INIT;

    return d;
}


void drive_free(drive_t *d)
{
    if (!d) return;

    spinlock_acquire(&d->device.slock);
    spinlock_acquire(&drives_lock);

    if(d->device.bus)
        d->device.bus->remove_device(&d->device);

    spinlock_release(&d->device.slock);
    spinlock_release(&drives_lock);

    ref_put(&d->device.refcount);
    heap_free(d);
}


void drive_mount(drive_t *d)
{
    spinlock_acquire(&d->device.slock);
    spinlock_acquire(&drives_lock);

    if (_drive_count >= MAX_DRIVES)
    {
        log(LOG_ERROR, "Max drive number reached");
        spinlock_release(&d->device.slock);
        spinlock_release(&drives_lock);
        return;
    }

    d->id = _drive_count;
    drives[_drive_count++] = d;

    if (d->device.bus)
        bus_register(d->device.bus);

    ref_get(&d->device.refcount);

    spinlock_release(&d->device.slock);
    spinlock_release(&drives_lock);
}

void drive_unmount(drive_t *d)
{
    spinlock_acquire(&d->device.slock);
    spinlock_acquire(&drives_lock);

    for (int i = 0; i < _drive_count; i++)
    {
        if (drives[i] == d)
        {
            for (int j=i; j < _drive_count - 1; j++)
            {
                drives[j] = drives[j+1];
                drives[j]->id = j;
            }
            drives[--_drive_count] = NULL;
            break;
        }
    }
    ref_put(&d->device.refcount);

    spinlock_release(&d->device.slock);
    spinlock_release(&drives_lock);
}

// Helpers

drive_t *drive_get(int id)
{
    spinlock_acquire(&drives_lock);

    if (id < 0 || id >= _drive_count)
    {
        spinlock_release(&drives_lock);
        return NULL;
    }

    drive_t *d = drives[id];
    ref_get(&d->device.refcount);

    spinlock_release(&drives_lock);
    return d;
}

int drive_count(void)
{
    int count;

    spinlock_acquire(&drives_lock);
    count = _drive_count;
    spinlock_release(&drives_lock);

    return count;
}
