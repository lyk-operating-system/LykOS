#include "mm/vm/vm_object.h"

#include "assert.h"
#include "hhdm.h"
#include "mm/mm.h"
#include "mm/pm.h"

static bool get_page(vm_object_t *obj, size_t offset, page_t **page_out)
{
    // Check local cache aka already COWed pages.
    page_t *page = xa_get(&obj->cached_pages, offset);
    if (page)
    {
        *page_out = page;
        return true;
    }

    // Page miss -> delegate to parent
    ASSERT(obj->source.shadow.parent);
    return obj->source.shadow.parent->ops->get_page(obj->source.shadow.parent, offset, page_out);
}

static bool put_page(vm_object_t *obj, page_t *page)
{
    // Sync the backing parent.
    ASSERT(obj->source.shadow.parent);
    return obj->source.shadow.parent->ops->put_page(obj->source.shadow.parent, page);
}

// Called during a write fault to perform COW
static bool copy_page(vm_object_t *obj, size_t offset, page_t *src, page_t **dst_out)
{
    page_t *dst = pm_alloc(0);
    if (!dst)
        return false;

    memcpy((void *)(dst->addr + HHDM), (void *)(src->addr + HHDM), ARCH_PAGE_GRAN);
    if (!xa_insert(&obj->cached_pages, offset, dst))
    {
        pm_free(dst);
        return false;
    }
    *dst_out = dst;

    return true;
}

static void destroy(vm_object_t *obj)
{
    void *ptr;
    size_t index = 0;
    xa_foreach(&obj->cached_pages, index, ptr)
        pm_free((page_t *)ptr);

    // Drop reference to parent
    ASSERT(obj->source.shadow.parent);
    vm_object_unref(obj->source.shadow.parent);
}

vm_object_ops_t shadow_ops = {
    .get_page = get_page,
    .put_page = put_page,
    .copy_page = copy_page,
    .destroy = destroy,
};
