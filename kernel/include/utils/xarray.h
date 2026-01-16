#pragma once

#include <stdint.h>
#include <stddef.h>

#define XA_SHIFT 6u // This will result in each node having 2^6=64 children.
#define XA_FANOUT (1u << XA_SHIFT)
#define XA_LEVELS ((sizeof(size_t) + XA_SHIFT - 1u) / XA_SHIFT)
#define XA_MASK (XA_FANOUT - 1u)

typedef unsigned xa_mark_t;

#define XA_MARK_0 0
#define XA_MARK_1 1
#define XA_MARK_2 2

typedef struct
{
    void *slots[XA_FANOUT];
    size_t not_null_count;
    uint64_t mark[3];
}
xa_node_t;

typedef struct
{
    xa_node_t *root;
}
xarray_t;

#define XARRAY_INIT \
    (xarray_t) { .root = NULL }

void *xa_get(const xarray_t *xa, size_t index);
bool xa_insert(xarray_t *xa, size_t index, void *value);
void *xa_remove(xarray_t *xa, size_t index);

bool xa_get_mark(xarray_t *xa, size_t index, xa_mark_t mark);
void xa_set_mark(xarray_t *xa, size_t index, xa_mark_t mark);
void xa_clear_mark(xarray_t *xa, size_t index, xa_mark_t mark);
void *xa_find_mark(xarray_t *xa, size_t *out_index, void **out_value, xa_mark_t mark);
