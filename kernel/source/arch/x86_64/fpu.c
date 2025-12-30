#include "arch/x86_64/fpu.h"

#include "arch/x86_64/cpuid.h"
#include "panic.h"
#include <stdint.h>

size_t x86_64_fpu_area_size = 0;
void (*x86_64_fpu_save)(void *area) = NULL;
void (*x86_64_fpu_restore)(void *area) = NULL;

static inline void fxsave(void *area)
{
    asm volatile("fxsave (%0)" : : "r"(area) : "memory");
}

static inline void fxrstor(void *area)
{
    asm volatile("fxrstor (%0)" : : "r"(area) : "memory");
}

static inline void xsave(void *area)
{
    asm volatile("xsave (%0)" : : "r"(area), "a"(0xFFFF'FFFF), "d"(0xFFFF'FFFF) : "memory");
}

static inline void xrstor(void *area)
{
    asm volatile("xrstor (%0)" : : "r"(area), "a"(0xFFFF'FFFF), "d"(0xFFFF'FFFF) : "memory");
}

void x86_64_fpu_init()
{
    if (!x86_64_cpuid_check_feature(X86_64_CPUID_FEATURE_FXSR))
        panic("FPU: FXSAVE and FXRSTOR instructions are not supported!");

    if(x86_64_cpuid_check_feature(X86_64_CPUID_FEATURE_XSAVE))
    {
        size_t area_size = x86_64_cpuid(0xD, 0).ecx;
        x86_64_fpu_area_size = area_size;
        x86_64_fpu_save = xsave;
        x86_64_fpu_restore = xrstor;
    }
    else
    {
        x86_64_fpu_area_size = 512;
        x86_64_fpu_save = fxsave;
        x86_64_fpu_restore = fxrstor;
    }
}

void x86_64_fpu_init_cpu()
{
    volatile uint64_t cr0, cr4;

    // Enable x87
    asm volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1 << 2); // CR0.EM
    cr0 |= 1 << 1;    // CR0.MP
    asm volatile ("mov %0, %%cr0" :: "r"(cr0) : "memory");

    // Enable MMX
    asm volatile ("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= 1 << 9;  // CR4.OSFXSR
    cr4 |= 1 << 10; // CR4.OSXMMEXCPT
    asm volatile ("mov %0, %%cr4" :: "r"(cr4) : "memory");

    if(x86_64_cpuid_check_feature(X86_64_CPUID_FEATURE_XSAVE))
    {
        asm volatile ("mov %%cr4, %0" : "=r"(cr4));
        cr4 |= 1 << 18; // CR4.OSXSAVE
        asm volatile ("mov %0, %%cr4" :: "r"(cr4) : "memory");

        uint64_t xcr0 = 0;
        xcr0 |= 1 << 0; // XCR0.X87
        xcr0 |= 1 << 1; // XCR0.SSE

        // Enable AVX
        if(x86_64_cpuid_check_feature(X86_64_CPUID_FEATURE_AVX))
            xcr0 |= 1 << 2; // XCR0.AVX

        //TODO: AVX512 support

        asm volatile("xsetbv" : : "a"(xcr0), "d"(xcr0 >> 32), "c"(0) : "memory");
    }
}
