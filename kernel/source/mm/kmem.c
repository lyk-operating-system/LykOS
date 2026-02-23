#include "mm/kmem.h"

#include "arch/types.h"
#include "hhdm.h"
#include "mm/pm.h"
#include "sys/smp.h"
#include "sys/thread.h"
#include "sys/sched.h"
#include "utils/list.h"

static kmem_slab_t *cache_make_slab(kmem_cache_t *cache)
{
    kmem_slab_t *slab = (kmem_slab_t *)(pm_alloc(0)->addr + HHDM);

    slab->cache = cache;
    slab->freelist = NULL;

    size_t slab_content_size = pm_order_to_pagecount(0) * ARCH_PAGE_GRAN - sizeof(kmem_slab_t);
    size_t object_count = slab_content_size / cache->object_size;
    for (size_t i = 0; i < object_count; i++)
    {
        void **obj = (void **)((uintptr_t)slab + sizeof(kmem_slab_t) + (i * cache->object_size));
        *obj = slab->freelist;
        slab->freelist = obj;
    }

    return slab;
}

static void *cache_alloc_from_slabs(kmem_cache_t *cache)
{
    if (list_is_empty(&cache->slabs_partial))
        list_append(&cache->slabs_partial, &cache_make_slab(cache)->list_node);

    kmem_slab_t *slab = LIST_GET_CONTAINER(LIST_FIRST(&cache->slabs_partial), kmem_slab_t, list_node);

    void *obj = slab->freelist;
    slab->freelist = *(void **)obj;

    if (slab->freelist == NULL)
    {
        list_remove(&cache->slabs_partial, &slab->list_node);
        list_append(&cache->slabs_full, &slab->list_node);
    }

    return obj;
}

static kmem_magazine_t *cache_make_magazine(kmem_cache_t *cache, bool populate)
{
    kmem_magazine_t *mag = (kmem_magazine_t *)(pm_alloc(0)->addr + HHDM);

    spinlock_acquire(&cache->slabs_lock);
    for (size_t i = 0; i < MAG_SIZE; i++)
        mag->objects[i] = populate ? cache_alloc_from_slabs(cache) : NULL;
    spinlock_release(&cache->slabs_lock);

    mag->count = populate ? MAG_SIZE : 0;
    mag->list_node = LIST_NODE_INIT;

    return mag;
}

void *kmem_alloc_cache(kmem_cache_t *cache)
{
    size_t cpu_id = sched_get_curr_thread()->assigned_cpu->id;
    kmem_cpu_cache_t *cpu_cache = &cache->cpu_cache[cpu_id];

    kmem_magazine_t *mag = cpu_cache->loaded;
    if (mag->count > 0)
        return mag->objects[--mag->count];

    if (cpu_cache->previous->count == MAG_SIZE)
    {
        cpu_cache->loaded = cpu_cache->previous;
        cpu_cache->previous = mag;
        return cpu_cache->loaded->objects[--cpu_cache->loaded->count];
    }

    spinlock_acquire(&cache->magazines_lock);
    mag = LIST_GET_CONTAINER(list_pop_head(&cache->magazines_full), kmem_magazine_t, list_node);
    if (mag)
    {
        list_append(&cache->magazines_empty, &cpu_cache->previous->list_node);
        cpu_cache->previous = cpu_cache->loaded;
        cpu_cache->loaded = mag;
        spinlock_release(&cache->magazines_lock);
        return mag->objects[--mag->count];
    }
    spinlock_release(&cache->magazines_lock);

    // If the depot has no full magazines then we directly allocate from a slab.

    spinlock_acquire(&cache->slabs_lock);
    void *obj = cache_alloc_from_slabs(cache);
    spinlock_release(&cache->slabs_lock);

    return obj;
}

void kmem_free_cache(kmem_cache_t *cache, void *obj)
{
    size_t cpu_id = sched_get_curr_thread()->assigned_cpu->id;
    kmem_cpu_cache_t *cpu_cache = &cache->cpu_cache[cpu_id];

    kmem_magazine_t *mag = cpu_cache->loaded;
    if (mag->count < MAG_SIZE)
    {
        mag->objects[mag->count++] = obj;
        return;
    }

    if (cpu_cache->previous->count == 0)
    {
        cpu_cache->loaded = cpu_cache->previous;
        cpu_cache->previous = mag;
        cpu_cache->loaded->objects[cpu_cache->loaded->count++] = obj;
        return;
    }

    spinlock_acquire(&cache->magazines_lock);
    list_append(&cache->magazines_full, &mag->list_node);
    kmem_magazine_t *new_mag = LIST_GET_CONTAINER(list_pop_head(&cache->magazines_full), kmem_magazine_t, list_node);
    spinlock_release(&cache->magazines_lock);

    // If the depot has no empty magazines then we create a new magazine.

    if (!new_mag)
        new_mag = cache_make_magazine(cache, false);

    cpu_cache->previous = cpu_cache->loaded;
    cpu_cache->loaded = new_mag;
    new_mag->count = 0;
    new_mag->objects[new_mag->count++] = obj;
}

kmem_cache_t *kmem_new_cache(const char *name, size_t size)
{
    kmem_cache_t *cache = (kmem_cache_t *)(pm_alloc(0)->addr + HHDM);

    *cache = (kmem_cache_t) {
        .name = name,
        .object_size = size,
        .slabs_full = LIST_INIT,
        .slabs_partial = LIST_INIT,
        .slabs_lock = SPINLOCK_INIT,
        .magazines_full = LIST_INIT,
        .magazines_empty = LIST_INIT,
        .magazines_lock = SPINLOCK_INIT
    };

    for (int i = 0; i < MAX_CPUS; i++)
    {
        cache->cpu_cache[i] = (kmem_cpu_cache_t) {
            .loaded = cache_make_magazine(cache, true),
            .previous = cache_make_magazine(cache, false)
        };
    }

    return cache;
}
