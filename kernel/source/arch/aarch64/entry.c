#include "arch/lcpu.h"
#include "bootreq.h"
#include "dev/acpi/acpi.h"
#include "gfx/simplefb.h"
#include "log.h"
#include "mm/heap.h"
#include "mm/pm.h"
#include "mm/vm.h"
#include "proc/smp.h"
#include "proc/thread.h"

#include "arch/aarch64/devices/gic.h"
#include "arch/aarch64/int.h"
#include <stdint.h>

uintptr_t HHDM;

[[noreturn]] extern void kernel_main();

static cpu_t early_cpu = (cpu_t) {
    .id = 0,
};

static thread_t early_thread = (thread_t) {
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

    // EVT
    aarch64_int_init_cpu();

    // Memory
    pm_init();
    heap_init();
    vm_init();

    // ACPI
    acpi_init();

    // GIC
    aarch64_gic_detect(); // This needs ACPI
    aarch64_gic->gicd_init();

    // Also initialize CPU specific code for the bootstrap CPU.
    // This step is going to be done again during SMP initialization.
    arch_lcpu_init();

    kernel_main();
}
