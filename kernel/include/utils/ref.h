#pragma once

#include <stddef.h>
#include <stdatomic.h>

struct ref;

typedef struct ref
{
    atomic_int count;
}
ref_t;

static inline void ref_init(ref_t *r)
{
    atomic_store(&r->count, 1);
}

static inline void ref_get(ref_t *r)
{
    atomic_fetch_add(&r->count, 1);
}

static inline bool ref_put(ref_t *r)
{
    if (atomic_fetch_sub(&r->count, 1) == 1)
        return true;
    return false;
}

static inline int ref_read(ref_t *r)
{
    return atomic_load(&r->count);
}
