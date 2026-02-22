#include "mm/vm/object.h"
#include "mm/pm.h"

static bool get_page(vm_object_t *obj, size_t offset, page_t **page_out)
{
    uintptr_t paddr = obj->source.phys.paddr + offset;
    page_t *page = pm_phys_to_page(paddr);
    if (!page)
        return false;

    *page_out = page;
    return true;
}

static bool put_page(vm_object_t *obj [[maybe_unused]],
                     page_t *page [[maybe_unused]])
{
    return true;
}

static bool copy_page(vm_object_t *obj [[maybe_unused]],
                      size_t offset [[maybe_unused]],
                      page_t *src [[maybe_unused]],
                      page_t **dst_out [[maybe_unused]])
{
    // Not meant to use for this
    return false;
}

static void destroy(vm_object_t *obj [[maybe_unused]])
{
    // Does nothing lol
}

vm_object_ops_t phys_ops = {
    .get_page = get_page,
    .put_page = put_page,
    .copy_page = copy_page,
    .destroy = destroy,
};
