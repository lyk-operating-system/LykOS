#pragma once

#include <stddef.h>

#define KIB 1024ull
#define MIB (KIB * 1024ull)
#define GIB (MIB * 1024ull)

#define MM_PROT_WRITE   0x1
#define MM_PROT_USER    0x2
#define MM_PROT_EXEC    0x4
#define MM_PROT_UC      0x8
#define MM_PROT_FULL    (MM_PROT_WRITE | MM_PROT_USER | MM_PROT_EXEC)

typedef struct
{
    bool read  : 1;
    bool write : 1;
    bool exec  : 1;
}
vm_protection_t;

#define VM_PROTECTION_FULL ((vm_protection_t) {.read = true, .write = true, .exec = true})

typedef enum
{
    VM_PRIVILEGE_KERNEL,
    VM_PRIVILEGE_USER
}
vm_privilege_t;

typedef enum
{
    VM_CACHE_STANDARD,      // Write-Back
    VM_CACHE_WRITE_THROUGH, // Write-Through
    VM_CACHE_WRITE_COMBINE, // Write-Combining
    VM_CACHE_NONE           // Uncached
}
vm_cache_t;

void *memcpy(void *restrict dest, const void *restrict src, size_t n);

void *memmove(void *dest, const void *src, size_t n);

int memcmp(const void *s1, const void *s2, size_t n);

void *memset(void *s, int c, size_t n);
