#include "mm/vm/vm_object.h"

#include "hhdm.h"
#include "mm/mm.h"
#include "mm/pm.h"

static bool anon_get_page(vm_object_t *obj, size_t offset,
                          [[maybe_unused]] uint32_t fault_flags,
                          page_t **page_out)
{
    spinlock_acquire(&obj->slock);

    // Check if the page is already resident
    page_t *page = vm_object_lookup_page(obj, offset);
    if (page)
    {
        *page_out = page;
        spinlock_release(&obj->slock);
        return true;
    }

    // Not resident: Allocate a new physical page
    page = pm_alloc(0);
    if (!page)
    {
        spinlock_release(&obj->slock);
        return false; // Out of memory
    }

    // Anonymous memory must be zero-filled
    memset((void *)(page->addr + HHDM), 0, ARCH_PAGE_GRAN);

    // Cache it for future lookups
    vm_object_insert_page(obj, page, offset);

    *page_out = page;
    spinlock_release(&obj->slock);
    return true;
}

static bool anon_put_page([[maybe_unused]] vm_object_t *obj,
                          [[maybe_unused]] page_t *page)
{
    return true;
}

static void anon_destroy(vm_object_t *obj)
{
    void *ptr;
    size_t index = 0;

    xa_foreach(&obj->cached_pages, index, ptr)
        pm_free((page_t *)ptr);
}

vm_object_ops_t anon_ops = {
    .get_page  = anon_get_page,
    .put_page  = anon_put_page,
    .destroy   = anon_destroy
};
