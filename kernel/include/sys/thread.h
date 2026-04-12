#pragma once

#include "arch/thread.h"
#include "sys/proc.h"
#include "utils/list.h"
#include "utils/ref.h"
#include <stdint.h>

typedef struct smp_cpu cpu_t;
typedef struct proc proc_t;
typedef struct thread thread_t;

typedef enum
{
    THREAD_STATE_NEW,
    THREAD_STATE_READY,
    THREAD_STATE_RUNNING,
    THREAD_STATE_BLOCKED,
    THREAD_STATE_TERMINATED,
    THREAD_STATE_SLEEPING,
}
thread_status_t;

struct thread
{
    arch_thread_context_t context;

    uint32_t tid;
    proc_t *owner;

    thread_status_t status;
    cpu_t *assigned_cpu;

    unsigned priority;
    uint64_t last_ran;
    uint64_t sleep_until;
    uint64_t last_boost;

    list_node_t proc_thread_list_node;
    list_node_t sched_thread_list_node;
    ref_t refcount;
    spinlock_t slock;
};

int thread_create_kernel(vm_addrspace_t *as, uintptr_t entry, size_t stack_size,
                         thread_t **out_thread);

int thread_create_user(vm_addrspace_t *as, uintptr_t entry, size_t stack_size,
                       const char *const argv[], const char *const envp[],
                       thread_t **out_thread);

void thread_destroy(thread_t *thread);

[[nodiscard]] thread_t *thread_duplicate(thread_t *thread);
