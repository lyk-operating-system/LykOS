#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct
{
    void *vaddr;
    uintptr_t paddr;
    uint8_t order;
    size_t size;
} dma_buf_t;

dma_buf_t dma_alloc(size_t size);
void      dma_free(dma_buf_t *b);
