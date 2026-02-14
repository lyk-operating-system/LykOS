#include "mm/vm.h"

#include "hhdm.h"
#include "mm/pm.h"

static page_t *anon_fault(vm_object_t *obj, size_t offset, uintptr_t addr, unsigned type)
{
    page_t *page = pm_alloc(0);
    if (!page)
        return NULL;

    memset((void *)(page->addr + HHDM), 0, ARCH_PAGE_GRAN);

    xa_insert(&obj->cached_pages, offset, page);

    return page;
}

static void anon_destroy(vm_object_t *obj)
{

}

vm_object_ops_t anon_ops = {
    .fault = anon_fault,
    .destroy  = anon_destroy
};
