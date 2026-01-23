#include "crypto/hash.h"

size_t djb2(const char *str)
{
    size_t hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; // hash * 33 + c

    return hash;
}

size_t djb2_len(const char *str, size_t len)
{
    size_t hash = 5381;

    while (len--)
        hash = ((hash << 5) + hash) + *str++; // hash * 33 + c

    return hash;
}
