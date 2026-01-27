#include "arch/lcpu.h"
#include "bootreq.h"
#include "dev/acpi/acpi.h"
#include "gfx/simplefb.h"
#include "hhdm.h"
#include "log.h"
#include "mm/heap.h"
#include "mm/pm.h"
#include "mm/vm.h"
#include "proc/smp.h"
#include "proc/thread.h"

#include "arch/x86_64/devices/hpet.h"
#include "arch/x86_64/devices/ioapic.h"
#include "arch/x86_64/fpu.h"
#include "arch/x86_64/tables/gdt.h"
#include "arch/x86_64/tables/idt.h"

uintptr_t HHDM;

[[noreturn]] extern void kernel_main();

static cpu_t early_cpu = (cpu_t) {
    .id = 0,
};

static thread_t early_thread = (thread_t) {
    .context = (arch_thread_context_t) {
        .self = &early_thread.context
    },
    .tid = 0,
    .assigned_cpu = &early_cpu
};

void __entry()
{
    HHDM = bootreq_hhdm.response->offset;
    // Load pseudo-thread
    arch_lcpu_thread_reg_write((size_t)&early_thread.context);

    simplefb_init();
    log(LOG_INFO, "Kernel compiled on %s at %s.", __DATE__, __TIME__);

    // Tables
    x86_64_gdt_init_cpu();
    x86_64_idt_init();
    x86_64_idt_init_cpu();

    // FPU
    x86_64_fpu_init();

    // Memory
    pm_init();
    heap_init();
    vm_init();

    // ACPI
    acpi_init();

    // IOAPIC
    x86_64_ioapic_init(); // requires acpi
    // HPET
    x86_64_hpet_init(); // requires acpi

    kernel_main();
}
