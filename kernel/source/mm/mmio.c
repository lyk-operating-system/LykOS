#include "mm/mm.h"
#include "mm/vm.h"
#include "mm/pm.h"
#include "hhdm.h"
#include "arch/paging.h"
#include "uapi/errno.h"
#include "utils/math.h"   // if you have CEIL/FLOOR macros
#include "log.h"

#define MMIO_MAP_FLAGS   (VM_MAP_ANON | VM_MAP_POPULATE)
#define MMIO_MAP_PROT    (MM_PROT_WRITE | MM_PROT_UC)

/**
 * Map a physical MMIO range into kernel virtual memory (uncached).
 *
 * @param phys  Physical base address (can be unaligned)
 * @param size  Bytes
 * @return      Virtual address that corresponds to the original phys (incl. offset), or 0 on failure.
 */
uintptr_t mmio_map(uintptr_t phys, size_t size)
{
    if (size == 0)
        return 0;

    // Preserve sub-page offset, map whole pages.
    const uintptr_t page_off  = phys & (ARCH_PAGE_GRAN - 1);
    const uintptr_t phys_base = phys & ~(ARCH_PAGE_GRAN - 1);
    const size_t    map_len   = CEIL(size + page_off, ARCH_PAGE_GRAN);

    vm_protection_t prot = VM_PROTECTION_FULL;
    prot.exec = 0; // NX for MMIO

    uintptr_t virt_base = 0;

    int rc = vm_map_phys(vm_kernel_as,
                         0,
                         phys_base,
                         map_len,
                         prot,
                         VM_CACHE_NONE,    // UC MMIO
                         0,
                         &virt_base);

    if (rc != EOK || virt_base == 0) {
        log(LOG_ERROR, "mmio_map: vm_map_phys failed rc=%d phys=%#lx len=%#lx",
            rc, (unsigned long)phys_base, (unsigned long)map_len);
        return 0;
    }

    return virt_base + page_off;
}

void mmio_unmap(uintptr_t vaddr, size_t size)
{
    if (!vaddr || !size)
        return;

    const uintptr_t page_off  = vaddr & (ARCH_PAGE_GRAN - 1);
    const uintptr_t virt_base = vaddr & ~(ARCH_PAGE_GRAN - 1);
    const size_t    map_len   = CEIL(size + page_off, ARCH_PAGE_GRAN);

    // Unmap the VA range; for MMIO we never "free" device physical pages.
    vm_unmap(vm_kernel_as, virt_base, map_len);
}
