#include "mm/vm/vm_object.h"

#include "assert.h"
#include "mm/heap.h"
#include "mm/mm.h"

extern vm_object_ops_t anon_ops;
extern vm_object_ops_t phys_ops;
extern vm_object_ops_t shadow_ops;

static vm_object_ops_t *ops_table[] = {
    [VM_OBJ_ANON]   = &anon_ops,
    [VM_OBJ_PHYS]   = &phys_ops,
    [VM_OBJ_SHADOW] = &shadow_ops
};

/*
 * Lifecycle
 */

vm_object_t *vm_object_create(vm_object_type_t type, size_t size)
{
    vm_object_t *obj = heap_alloc(sizeof(vm_object_t));
    if (!obj)
        return NULL;

    obj->type = type;
    obj->size = size;
    obj->cached_pages = XARRAY_INIT;
    obj->ops = ops_table[type];
    memset(&obj->source, 0, sizeof(obj->source));
    obj->slock = SPINLOCK_INIT;
    ref_init(&obj->refcount);

    return obj;
}

void vm_object_destroy(vm_object_t *obj)
{
    ASSERT(ref_read(&obj->refcount) == 1);

    obj->ops->destroy(obj);

    heap_free(obj);
}

void vm_object_ref(vm_object_t *obj)
{
    ref_get(&obj->refcount);
}

void vm_object_unref(vm_object_t *obj)
{
    if (ref_put(&obj->refcount))
        vm_object_destroy(obj);
}

/*
 * Page Management
 */

bool vm_object_get_page(vm_object_t *obj, size_t offset, page_t **page_out)
{
    spinlock_acquire(&obj->slock);

    page_t *page = xa_get(&obj->cached_pages, offset);
    if (page)
    {
        *page_out = page;
        spinlock_release(&obj->slock);
        return true;
    }

    // Cache miss
    return obj->ops->get_page(obj, offset, page_out);

    spinlock_release(&obj->slock);
}

void vm_object_insert_page(vm_object_t *obj, page_t *page, size_t offset)
{
    spinlock_acquire(&obj->slock);
    xa_insert(&obj->cached_pages, offset, page);
    spinlock_release(&obj->slock);
}

void vm_object_remove_page(vm_object_t *obj, size_t offset)
{
    spinlock_acquire(&obj->slock);
    xa_remove(&obj->cached_pages, offset);
    spinlock_release(&obj->slock);
}

page_t *vm_object_lookup_page(vm_object_t *obj, size_t offset)
{
    spinlock_acquire(&obj->slock);
    page_t *page = xa_get(&obj->cached_pages, offset);
    spinlock_release(&obj->slock);

    return page;
}

/*
 * Sync
 */

int vm_object_sync(vm_object_t *obj, size_t start, size_t end)
{
    panic("TODO");
}
