#include "mm/vm/vm_object.h"

#include "mm/pm.h"

static bool phys_get_page(vm_object_t *obj, size_t offset,
                          [[maybe_unused]] uint32_t fault_flags,
                          page_t **page_out)
{
    uintptr_t paddr = obj->source.phys.paddr + offset;

    *page_out = pm_phys_to_page(paddr);
    return *page_out != NULL;
}

static bool phys_put_page([[maybe_unused]] vm_object_t *obj,
                          [[maybe_unused]] page_t *page)
{
    return true;
}

static void phys_destroy([[maybe_unused]] vm_object_t *obj)
{
    // Does nothing lol
}

vm_object_ops_t phys_ops = {
    .get_page = phys_get_page,
    .put_page = phys_put_page,
    .destroy = phys_destroy
};
