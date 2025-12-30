#pragma once

#include <stdint.h>
#include <stddef.h>

#define XA_SHIFT 6u
#define XA_FANOUT (1u << XA_SHIFT)
#define XA_LEVELS ((sizeof(size_t) + XA_SHIFT - 1u) / XA_SHIFT)
#define XA_MASK (XA_FANOUT - 1u)

typedef struct
{
    void *slots[XA_FANOUT];
    uint16_t not_null_count;
}
xa_node_t;

typedef struct
{
    xa_node_t *root;
}
xarray_t;

#define XARRAY_INIT \
    (xarray_t) { .root = NULL }

bool xa_insert(xarray_t *xa, size_t index, void *value);
void *xa_remove(xarray_t *xa, size_t index);

void *xa_get(const xarray_t *xa, size_t index);
