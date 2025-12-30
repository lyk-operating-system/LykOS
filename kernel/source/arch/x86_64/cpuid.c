#include "arch/x86_64/cpuid.h"

#include "assert.h"

x86_64_cpuid_response_t x86_64_cpuid(uint32_t eax, uint32_t ecx)
{
    x86_64_cpuid_response_t ret;

    asm volatile("cpuid" : "=a"(ret.eax), "=b"(ret.ebx), "=c"(ret.ecx), "=d"(ret.edx) : "a"(eax), "c"(ecx));

    return ret;
}

bool x86_64_cpuid_check_feature(x86_64_cpuid_feature_t feature)
{
    uint32_t value;

    x86_64_cpuid_response_t resp = x86_64_cpuid(feature.eax, feature.reg);

    switch(feature.reg)
    {
        case ECX: value = resp.ecx;  break;
        case EDX: value = resp.edx;  break;
        default: ASSERT(false); break;
    }

    return (bool)(value & (1 << feature.offset));
}
