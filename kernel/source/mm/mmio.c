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
    uintptr_t page_off   = phys & (ARCH_PAGE_GRAN - 1);
    uintptr_t phys_page  = phys & ~(ARCH_PAGE_GRAN - 1);
    size_t    size_pages = CEIL(size + page_off, ARCH_PAGE_GRAN);

    // 1) Reserve a VA range (this allocs+maps RAM pages right now).
    uintptr_t virt_base = 0;
    int rc = vm_map(vm_kernel_as,
                    0,
                    size_pages,
                    MMIO_MAP_PROT,
                    MMIO_MAP_FLAGS,
                    NULL,
                    0,
                    &virt_base);
    if (rc != EOK || virt_base == 0)
        return 0;

    // 2) Replace each VA page mapping with the target MMIO physical page.
    for (size_t off = 0; off < size_pages; off += ARCH_PAGE_GRAN)
    {
        uintptr_t va = virt_base + off;

        // Find the RAM page vm_map() created so we can free it.
        uintptr_t tmp_phys = 0;
        bool ok = arch_paging_vaddr_to_paddr(vm_kernel_as->page_map, va, &tmp_phys);
        if (!ok)
        {
            // Best-effort cleanup: unmap whole region
            vm_unmap(vm_kernel_as, virt_base, size_pages);
            return 0;
        }

        // Unmap the temp page (flushes TLB for this VA in your arch_paging_unmap_page()).
        arch_paging_unmap_page(vm_kernel_as->page_map, va);

        // Free the temp physical page (vm_map allocated it via pm_alloc(0)).
        // This assumes your pm_alloc sets refcount=1 and your mappings don't bump it.
        pm_free(pm_phys_to_page(tmp_phys & ~(ARCH_PAGE_GRAN - 1)));

        // Now map the MMIO physical page uncached.
        arch_paging_map_page(vm_kernel_as->page_map,
                             va,
                             phys_page + off,
                             ARCH_PAGE_GRAN,
                             MMIO_MAP_PROT);
        // No invlpg needed here because we already invalidated when unmapping.
    }

    // Return VA corresponding to original physical (including offset)
    return virt_base + page_off;
}

void mmio_unmap(uintptr_t vaddr, size_t size)
{
    if (!vaddr || !size) return;

    uintptr_t page_off   = vaddr & (ARCH_PAGE_GRAN - 1);
    uintptr_t virt_base  = vaddr & ~(ARCH_PAGE_GRAN - 1);
    size_t    size_pages = CEIL(size + page_off, ARCH_PAGE_GRAN);

    vm_unmap(vm_kernel_as, virt_base, size_pages);
}
