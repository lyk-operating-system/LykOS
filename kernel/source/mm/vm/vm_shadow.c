#include "mm/vm/vm_object.h"

#include "assert.h"
#include "hhdm.h"
#include "mm/mm.h"
#include "mm/pm.h"

static bool shadow_get_page(vm_object_t *obj, size_t offset, uint32_t fault_flags, page_t **page_out)
{
    spinlock_acquire(&obj->slock);

    // Check if the shadow object already has a private, modified copy of the page.
    page_t *page = vm_object_lookup_page(obj, offset);
    if (page)
    {
        *page_out = page;
        spinlock_release(&obj->slock);
        return true;
    }

    // If not, fetch the page from the parent object.
    vm_object_t *parent = obj->source.shadow.parent;
    ASSERT(parent);
    size_t parent_offset = offset + obj->source.shadow.offset;
    page_t *parent_page = NULL;

    spinlock_release(&obj->slock);

    // Ask the parent for a READ fault, we do not want it to COW its own pages.
    if (!parent->ops->get_page(parent, parent_offset, VM_FAULT_READ, &parent_page))
        return false;

    // If the proc doesn't want to modify return the parent's page (for read-only usage).
    if (fault_flags != VM_FAULT_WRITE)
    {
        *page_out = parent_page;
        return true;
    }

    // For write faults perform COW.
    page = pm_alloc(0);
    if (!page)
        return false; // OUT OF MEM
    memcpy(
        (void *)(page->addr + HHDM),
        (void *)(parent_page->addr + HHDM),
        ARCH_PAGE_GRAN
    );

    spinlock_acquire(&obj->slock);
    page_t *existing = vm_object_lookup_page(obj, offset);
    if (existing)
    {
        pm_free(page);
        *page_out = existing;
    }
    else
    {
        vm_object_insert_page(obj, page, offset);
        *page_out = page;
    }
    spinlock_release(&obj->slock);

    return true;
}

static bool shadow_put_page([[maybe_unused]] vm_object_t *obj,
                            [[maybe_unused]] page_t *page)
{
    return true;
}

static void shadow_destroy(vm_object_t *obj)
{
    void *ptr;
    size_t index = 0;
    xa_foreach(&obj->cached_pages, index, ptr)
        pm_free((page_t *)ptr);

    // Drop reference to parent.
    vm_object_unref(obj->source.shadow.parent);
}

vm_object_ops_t shadow_ops = {
    .get_page = shadow_get_page,
    .put_page = shadow_put_page,
    .destroy = shadow_destroy
};
