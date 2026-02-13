#pragma once

#include "panic.h"

/**
 * @brief Make an assertion & panic on failure.
 */
#define ASSERT(ASSERTION)                               \
    if (!(ASSERTION))                                   \
    {                                                   \
        panic("Assertion `%s` failed.", #ASSERTION);    \
    }

/**
 * @brief Make an assertion & panic with a comment on failure.
 */
#define ASSERT_C(ASSERTION, FORMAT, ...)        \
    if (!(ASSERTION))                           \
    {                                           \
        panic(FORMAT, ##__VA_ARGS__);           \
    }
// ##__VA_ARGS__ removes the comma when no extra args are provided.

#define STATIC_ASSERT_MSG(COND,MSG) typedef char static_assertion_##MSG[(!!(COND))*2-1]
#define STATIC_ASSERT3(X,L) STATIC_ASSERT_MSG(X,static_assertion_at_line_##L)
#define STATIC_ASSERT2(X,L) STATIC_ASSERT3(X,L)
#define STATIC_ASSERT(X)    STATIC_ASSERT2(X,__LINE__)
