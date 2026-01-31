#include "mm/dma.h"

uintptr_t dma_phys_addr(void *virt)
{
    return (uintptr_t)virt - HHDM;
}

uintptr_t dma_map(size_t size)
{
    size_t pages = (size + ARCH_PAGE_GRAN - 1) / ARCH_PAGE_GRAN;
    uint8_t order = pm_pagecount_to_order(pages);

    page_t *page = pm_alloc(order);
    if (!page) return 0;

    size_t count = pm_order_to_pagecount(order);
    for (size_t i = 0; i < count; i++)
        pm_page_map_inc(&page[i]);

    return page->addr + HHDM;
}

void dma_unmap(uintptr_t virt, size_t size)
{
    uintptr_t phys = (uintptr_t)virt - HHDM;

    size_t pages = (size + ARCH_PAGE_GRAN - 1) / ARCH_PAGE_GRAN;
    uint8_t order = pm_pagecount_to_order(pages);
    size_t count = pm_order_to_pagecount(order);

    page_t *page = pm_phys_to_page(phys);

    for (size_t i = 0; i < count; i++)
    {
        if (pm_page_map_dec(&page[i]))
            pm_free(&page[i]); // probs with refcount might arise
    }
}
