#pragma once

#include "arch/thread.h"
#include "proc.h"
#include "utils/list.h"
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

    size_t priority;
    thread_status_t status;
    uint64_t last_ran;
    uint64_t sleep_until;
    cpu_t *assigned_cpu;

    list_node_t proc_thread_list_node;
    list_node_t sched_thread_list_node;
    spinlock_t slock;
    size_t ref_count;
};

thread_t *thread_create(proc_t *proc, uintptr_t entry);

void thread_destroy(thread_t *thread);

[[nodiscard]] thread_t *thread_duplicate(thread_t *thread);
