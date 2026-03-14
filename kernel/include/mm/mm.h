#pragma once

#include <stddef.h>
#include <stdint.h>

#define KIB 1024ull
#define MIB (KIB * 1024ull)
#define GIB (MIB * 1024ull)

typedef uint8_t vm_protection_t;

#define VM_PROTECTION_READ      0x01
#define VM_PROTECTION_WRITE     0x02
#define VM_PROTECTION_EXECUTE   0x04
#define VM_PROTECTION_FULL (VM_PROTECTION_READ | VM_PROTECTION_WRITE | VM_PROTECTION_EXECUTE)

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

typedef enum
{
    VM_FAULT_READ,
    VM_FAULT_WRITE,
    VM_FAULT_INSTRUCTION_FETCH
}
vm_fault_type_t;

void *memcpy(void *restrict dest, const void *restrict src, size_t n);

void *memmove(void *dest, const void *src, size_t n);

int memcmp(const void *s1, const void *s2, size_t n);

void *memset(void *s, int c, size_t n);
