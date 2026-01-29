#include "dev/bus.h"

#include "log.h"
#include "utils/string.h"

static list_t bus_list = LIST_INIT;
static spinlock_t bus_list_slock = SPINLOCK_INIT;

bus_t *bus_get(const char *name)
{
    spinlock_acquire(&bus_list_slock);

    FOREACH(n, bus_list)
    {
        bus_t *bus = LIST_GET_CONTAINER(n, bus_t, list_node);
        if (strcmp(bus->name, name) == 0)
        {
            spinlock_release(&bus_list_slock);
            ref_get(&bus->refcount);
            return bus;
        }
    }

    spinlock_release(&bus_list_slock);
    return NULL;
}

void bus_put(bus_t *bus)
{
    ref_put(&bus->refcount);
}

bool bus_register(bus_t *bus)
{
    if (!bus || !bus->name)
        return false;

    bus->devices = LIST_INIT;
    bus->drivers = LIST_INIT;
    ref_init(&bus->refcount);
    bus->slock = SPINLOCK_INIT;

    spinlock_acquire(&bus_list_slock);

    FOREACH(n, bus_list)
    {
        bus_t *b = LIST_GET_CONTAINER(n, bus_t, list_node);
        if (strcmp(b->name, bus->name) == 0)
            return false;
    }
    list_append(&bus_list, &bus->list_node);

    spinlock_release(&bus_list_slock);

    log(LOG_INFO, "Bus registered: %s", bus->name);
    return true;
}
