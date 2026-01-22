#pragma once

#include <stddef.h>

#define KIB 1024ull
#define MIB (KIB * 1024ull)
#define GIB (MIB * 1024ull)

#define MM_PROT_WRITE   0x1
#define MM_PROT_USER    0x2
#define MM_PROT_EXEC    0x4
#define MM_PROT_FULL    (MM_PROT_WRITE | MM_PROT_USER | MM_PROT_EXEC)

void *memcpy(void *restrict dest, const void *restrict src, size_t n);

void *memmove(void *dest, const void *src, size_t n);

int memcmp(const void *s1, const void *s2, size_t n);

void *memset(void *s, int c, size_t n);
