#include "sys/smp.h"

#include "arch/lcpu.h"
#include "bootreq.h"
#include "log.h"
#include "mm/heap.h"
#include "panic.h"
#include "sys/proc.h"
#include "sys/sched.h"
#include "sys/thread.h"

list_t smp_cpus = LIST_INIT;
static proc_t *idle_proc;

static spinlock_t slock;

[[noreturn]] [[gnu::noinline]] static void thread_idle_func(struct limine_mp_info *mp_info)
{
    // Sequentially initializing CPU cores allows for easier debugging.
    spinlock_acquire(&slock);

    arch_lcpu_thread_reg_write((size_t)mp_info->extra_argument);
    arch_lcpu_init();
    log(LOG_INFO, "CPU #%02d initialized. Idling...", ((thread_t *)mp_info->extra_argument)->assigned_cpu->id);

    spinlock_release(&slock);

    while (true)
        sched_yield(THREAD_STATE_READY);
}

void smp_init()
{
    if (bootreq_mp.response == NULL)
        panic("Invalid SMP info provided by the bootloader!");

    idle_proc = proc_create("System Idle Process", "/", false);

    for (size_t i = 0; i < bootreq_mp.response->cpu_count; i++)
    {
        struct limine_mp_info *mp_info = bootreq_mp.response->cpus[i];

        thread_t *idle_thread = thread_create(idle_proc, (uintptr_t)&thread_idle_func);
        smp_cpu_t *cpu = heap_alloc(sizeof(smp_cpu_t));
        *cpu = (smp_cpu_t) {
            .id = i,
            .idle_thread = idle_thread,
            .curr_thread = idle_thread,
            .cpu_list_node = LIST_NODE_INIT
        };
        list_append(&smp_cpus, &cpu->cpu_list_node);
        idle_thread->assigned_cpu = cpu;

        mp_info->extra_argument = (uint64_t)idle_thread;
    }

    struct limine_mp_info *bsp_mp_info;
    for (size_t i = 0; i < bootreq_mp.response->cpu_count; i++)
    {
        struct limine_mp_info *mp_info = bootreq_mp.response->cpus[i];

#if defined(__x86_64__)
        if (mp_info->lapic_id == bootreq_mp.response->bsp_lapic_id)
#elif defined(__aarch64__)
        if (mp_info->mpidr == bootreq_mp.response->bsp_mpidr)
#endif
        {
            bsp_mp_info = mp_info;
            continue;
        }

        __atomic_store_n(&mp_info->goto_address, (limine_goto_address)&thread_idle_func, __ATOMIC_SEQ_CST);
    }

    // Also initialize the bootstrap processor.
    thread_idle_func(bsp_mp_info);
}
