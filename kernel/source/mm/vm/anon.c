#include "mm/vm/object.h"

#include "hhdm.h"
#include "mm/mm.h"
#include "mm/pm.h"

static bool anon_get_page(vm_object_t *obj, size_t offset, page_t **page_out)
{
    page_t *page = pm_alloc(0);
    if (!page)
        return false;

    memset((void *)(page->addr + HHDM), 0, ARCH_PAGE_GRAN);

    vm_object_insert_page(obj, page, offset);

    *page_out = page;
    return true;
}

static bool anon_copy_page(vm_object_t *obj, size_t offset, page_t *src, page_t **page_out)
{
    page_t *dst = pm_alloc(0);
    if (!dst)
        return false;

    memcpy((void *)(dst->addr + HHDM),
           (void *)(src->addr + HHDM),
           ARCH_PAGE_GRAN);

    vm_object_insert_page(obj, dst, offset);

    *page_out = dst;
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
    .put_page  = NULL,
    .copy_page = anon_copy_page,
    .destroy   = anon_destroy,
};
