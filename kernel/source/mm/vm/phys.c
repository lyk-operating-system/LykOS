#include "mm/vm.h"

static page_t *phys_fault(vm_object_t *obj, size_t offset, uintptr_t fault_addr, uint32_t flags)
{
    return NULL;
}

static void phys_destroy(vm_object_t *obj)
{

}

vm_object_ops_t phys_ops = {
    .fault = phys_fault,
    .destroy  = phys_destroy
};
