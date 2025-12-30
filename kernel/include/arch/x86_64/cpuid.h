#pragma once

#include <stdint.h>

typedef struct
{
    uint32_t eax;
    enum
    {
        ECX,
        EDX
    }
    reg;
    uint8_t offset;
}
x86_64_cpuid_feature_t;

typedef struct
{
    uint32_t eax, ebx, ecx, edx;
}
x86_64_cpuid_response_t;

#define X86_64_CPUID_FEATURE_FPU          ((x86_64_cpuid_feature_t) {1, EDX, 0})
#define X86_64_CPUID_FEATURE_VME          ((x86_64_cpuid_feature_t) {1, EDX, 1})
#define X86_64_CPUID_FEATURE_DE           ((x86_64_cpuid_feature_t) {1, EDX, 2})
#define X86_64_CPUID_FEATURE_PSE          ((x86_64_cpuid_feature_t) {1, EDX, 3})
#define X86_64_CPUID_FEATURE_TSC          ((x86_64_cpuid_feature_t) {1, EDX, 4})
#define X86_64_CPUID_FEATURE_MSR          ((x86_64_cpuid_feature_t) {1, EDX, 5})
#define X86_64_CPUID_FEATURE_PAE          ((x86_64_cpuid_feature_t) {1, EDX, 6})
#define X86_64_CPUID_FEATURE_MCE          ((x86_64_cpuid_feature_t) {1, EDX, 7})
#define X86_64_CPUID_FEATURE_CX8          ((x86_64_cpuid_feature_t) {1, EDX, 8})
#define X86_64_CPUID_FEATURE_APIC         ((x86_64_cpuid_feature_t) {1, EDX, 9})
#define X86_64_CPUID_FEATURE_MTRR         ((x86_64_cpuid_feature_t) {1, EDX, 10})
#define X86_64_CPUID_FEATURE_SEP          ((x86_64_cpuid_feature_t) {1, EDX, 11})
#define X86_64_CPUID_FEATURE_MTRR2        ((x86_64_cpuid_feature_t) {1, EDX, 12})
#define X86_64_CPUID_FEATURE_PGE          ((x86_64_cpuid_feature_t) {1, EDX, 13})
#define X86_64_CPUID_FEATURE_MCA          ((x86_64_cpuid_feature_t) {1, EDX, 14})
#define X86_64_CPUID_FEATURE_CMOV         ((x86_64_cpuid_feature_t) {1, EDX, 15})
#define X86_64_CPUID_FEATURE_PAT          ((x86_64_cpuid_feature_t) {1, EDX, 16})
#define X86_64_CPUID_FEATURE_PSE36        ((x86_64_cpuid_feature_t) {1, EDX, 17})
#define X86_64_CPUID_FEATURE_PSN          ((x86_64_cpuid_feature_t) {1, EDX, 18})
#define X86_64_CPUID_FEATURE_CLFSH        ((x86_64_cpuid_feature_t) {1, EDX, 19})
#define X86_64_CPUID_FEATURE_NX           ((x86_64_cpuid_feature_t) {1, EDX, 20})
#define X86_64_CPUID_FEATURE_DS           ((x86_64_cpuid_feature_t) {1, EDX, 21})
#define X86_64_CPUID_FEATURE_ACPI         ((x86_64_cpuid_feature_t) {1, EDX, 22})
#define X86_64_CPUID_FEATURE_MMX          ((x86_64_cpuid_feature_t) {1, EDX, 23})
#define X86_64_CPUID_FEATURE_FXSR         ((x86_64_cpuid_feature_t) {1, EDX, 24})
#define X86_64_CPUID_FEATURE_SSE          ((x86_64_cpuid_feature_t) {1, EDX, 25})
#define X86_64_CPUID_FEATURE_SSE2         ((x86_64_cpuid_feature_t) {1, EDX, 26})
#define X86_64_CPUID_FEATURE_SS           ((x86_64_cpuid_feature_t) {1, EDX, 27})
#define X86_64_CPUID_FEATURE_HTT          ((x86_64_cpuid_feature_t) {1, EDX, 28})
#define X86_64_CPUID_FEATURE_TM           ((x86_64_cpuid_feature_t) {1, EDX, 29})
#define X86_64_CPUID_FEATURE_IA64         ((x86_64_cpuid_feature_t) {1, EDX, 30})
#define X86_64_CPUID_FEATURE_PBE          ((x86_64_cpuid_feature_t) {1, EDX, 31})

#define X86_64_CPUID_FEATURE_SSE3         ((x86_64_cpuid_feature_t) {1, ECX, 0})
#define X86_64_CPUID_FEATURE_PCLMULQDQ    ((x86_64_cpuid_feature_t) {1, ECX, 1})
#define X86_64_CPUID_FEATURE_DTES64       ((x86_64_cpuid_feature_t) {1, ECX, 2})
#define X86_64_CPUID_FEATURE_MONITOR      ((x86_64_cpuid_feature_t) {1, ECX, 3})
#define X86_64_CPUID_FEATURE_DS_CPL       ((x86_64_cpuid_feature_t) {1, ECX, 4})
#define X86_64_CPUID_FEATURE_VMX          ((x86_64_cpuid_feature_t) {1, ECX, 5})
#define X86_64_CPUID_FEATURE_SMX          ((x86_64_cpuid_feature_t) {1, ECX, 6})
#define X86_64_CPUID_FEATURE_EST          ((x86_64_cpuid_feature_t) {1, ECX, 7})
#define X86_64_CPUID_FEATURE_TM2          ((x86_64_cpuid_feature_t) {1, ECX, 8})
#define X86_64_CPUID_FEATURE_SSSE3        ((x86_64_cpuid_feature_t) {1, ECX, 9})
#define X86_64_CPUID_FEATURE_CNXT_ID      ((x86_64_cpuid_feature_t) {1, ECX, 10})
#define X86_64_CPUID_FEATURE_SDBG         ((x86_64_cpuid_feature_t) {1, ECX, 11})
#define X86_64_CPUID_FEATURE_FMA          ((x86_64_cpuid_feature_t) {1, ECX, 12})
#define X86_64_CPUID_FEATURE_CX16         ((x86_64_cpuid_feature_t) {1, ECX, 13})
#define X86_64_CPUID_FEATURE_XTPR         ((x86_64_cpuid_feature_t) {1, ECX, 14})
#define X86_64_CPUID_FEATURE_PDCM         ((x86_64_cpuid_feature_t) {1, ECX, 15})
#define X86_64_CPUID_FEATURE_RESERVED1    ((x86_64_cpuid_feature_t) {1, ECX, 16})
#define X86_64_CPUID_FEATURE_PCID         ((x86_64_cpuid_feature_t) {1, ECX, 17})
#define X86_64_CPUID_FEATURE_DCA          ((x86_64_cpuid_feature_t) {1, ECX, 18})
#define X86_64_CPUID_FEATURE_SSE4_1       ((x86_64_cpuid_feature_t) {1, ECX, 19})
#define X86_64_CPUID_FEATURE_SSE4_2       ((x86_64_cpuid_feature_t) {1, ECX, 20})
#define X86_64_CPUID_FEATURE_X2APIC       ((x86_64_cpuid_feature_t) {1, ECX, 21})
#define X86_64_CPUID_FEATURE_MOVBE        ((x86_64_cpuid_feature_t) {1, ECX, 22})
#define X86_64_CPUID_FEATURE_POPCNT       ((x86_64_cpuid_feature_t) {1, ECX, 23})
#define X86_64_CPUID_FEATURE_TSC_DEADLINE ((x86_64_cpuid_feature_t) {1, ECX, 24})
#define X86_64_CPUID_FEATURE_AES_NI       ((x86_64_cpuid_feature_t) {1, ECX, 25})
#define X86_64_CPUID_FEATURE_XSAVE        ((x86_64_cpuid_feature_t) {1, ECX, 26})
#define X86_64_CPUID_FEATURE_OSXSAVE      ((x86_64_cpuid_feature_t) {1, ECX, 27})
#define X86_64_CPUID_FEATURE_AVX          ((x86_64_cpuid_feature_t) {1, ECX, 28})
#define X86_64_CPUID_FEATURE_F16C         ((x86_64_cpuid_feature_t) {1, ECX, 29})
#define X86_64_CPUID_FEATURE_RDRND        ((x86_64_cpuid_feature_t) {1, ECX, 30})
#define X86_64_CPUID_FEATURE_HYPERVISOR   ((x86_64_cpuid_feature_t) {1, ECX, 31})

#define X86_64_CPUID_FEATURE_AVX512       ((x86_64_cpuid_feature_t) {7, EBX, 16}

x86_64_cpuid_response_t x86_64_cpuid(uint32_t eax, uint32_t ecx);

bool x86_64_cpuid_check_feature(x86_64_cpuid_feature_t feature);
