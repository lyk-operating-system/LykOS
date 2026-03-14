#pragma once

#include <stddef.h>
#include <stdatomic.h>

#define REF_INIT 1

typedef atomic_int ref_t;

static inline void ref_inc(ref_t *r)
{
    atomic_fetch_add(r, 1);
}

static inline bool ref_dec(ref_t *r)
{
    if (atomic_fetch_sub(r, 1) == 1)
        return true;
    return false;
}

static inline int ref_read(ref_t *r)
{
    return atomic_load(r);
}
