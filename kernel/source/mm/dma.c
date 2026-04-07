#include "mm/dma.h"

#include "hhdm.h"
#include "mm/pm.h"
#include "mm/mm.h"

#define ALIGN_UP(x,a) (((x) + ((a)-1)) & ~((a)-1))

dma_buf_t dma_alloc(size_t size)
{
    dma_buf_t out = {0};

    size = ALIGN_UP(size, ARCH_PAGE_GRAN);
    if (size == 0) return out;

    size_t pages = size / ARCH_PAGE_GRAN;
    uint8_t order = pm_pagecount_to_order(pages);
    size_t count = pm_order_to_pagecount(order);

    page_t *page = pm_alloc(order);
    if (!page) return out;

    for (size_t i = 0; i < count; i++)
        pm_page_map_inc(&page[i]);

    out.paddr = page->addr;
    out.size  = count * ARCH_PAGE_GRAN;     // actual allocation size
    out.order = order;
    out.vaddr = (void *)(out.paddr + HHDM); // CPU VA through direct map

    return out;
}

void dma_free(dma_buf_t *b)
{
    if (!b || !b->paddr) return;

    size_t count = pm_order_to_pagecount(b->order);
    page_t *page = pm_phys_to_page(b->paddr);

    for (size_t i = 0; i < count; i++)
    {
        if (pm_page_map_dec(&page[i]))
            pm_free(&page[i]);
    }

    *b = (dma_buf_t){0};
}
