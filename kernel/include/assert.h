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
