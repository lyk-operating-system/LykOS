#include "mm/mm.h"

#include <stdint.h>

void *memcpy(void *restrict dest, const void *restrict src, size_t n)
{
    uint8_t *restrict d = dest;
    const uint8_t *restrict s = src;

    while (n >= sizeof(size_t))
    {
        *(size_t *)d = *(const size_t *)s;
        d += sizeof(size_t);
        s += sizeof(size_t);
        n -= sizeof(size_t);
    }

    while (n > 0)
    {
        *d = *s;
        d++;
        s++;
        n--;
    }

    return dest;
}

void *memmove(void *dest, const void *src, size_t n)
{
    uint8_t *d = dest;
    const uint8_t *s = src;

    if (d < s)
    {
        while (n >= sizeof(size_t))
        {
            *(size_t *)d = *(const size_t *)s;
            d += sizeof(size_t);
            s += sizeof(size_t);
            n -= sizeof(size_t);
        }

        while (n > 0)
        {
            *d = *s;
            d++;
            s++;
            n--;
        }
    }
    else if (d > s)
    {
        d += n;
        s += n;

        while (n >= sizeof(size_t))
        {
            d -= sizeof(size_t);
            s -= sizeof(size_t);
            *(size_t *)d = *(const size_t *)s;
            n -= sizeof(size_t);
        }

        while (n > 0)
        {
            d--;
            s--;
            *d = *s;
            n--;
        }
    }

    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
    const uint8_t *a = s1;
    const uint8_t *b = s2;

    while (n >= sizeof(size_t))
    {
        size_t x = *(const size_t *)a;
        size_t y = *(const size_t *)b;

        if (x != y)
            for (size_t i = 0; i < sizeof(size_t); i++)
                if (a[i] != b[i])
                    return a[i] < b[i] ? -1 : 1;

        a += sizeof(size_t);
        b += sizeof(size_t);
        n -= sizeof(size_t);
    }

    while (n > 0)
    {
        if (*a != *b)
            return *a < *b ? -1 : 1;

        a++;
        b++;
        n--;
    }

    return 0;
}

void *memset(void *dest, int c, size_t n)
{
    uint8_t *p = dest;
    size_t v = (uint8_t)c;

    size_t shift = 8;
    while (shift < sizeof(size_t) * 8)
    {
        v |= v << shift;
        shift <<= 1;
    }

    while (n >= sizeof(size_t))
    {
        *(size_t *)p = v;
        p += sizeof(size_t);
        n -= sizeof(size_t);
    }

    while (n > 0)
    {
        *p = (uint8_t)c;
        p++;
        n--;
    }

    return dest;
}
