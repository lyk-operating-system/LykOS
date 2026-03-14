#pragma once

#include "thread.h"
#include "utils/list.h"

typedef struct smp_cpu smp_cpu_t;
typedef struct proc proc_t;
typedef struct thread thread_t;

typedef struct smp_cpu
{
    size_t id;
    thread_t *idle_thread;
    thread_t *curr_thread;

    list_node_t cpu_list_node;
}
smp_cpu_t;

extern list_t smp_cpus;

void smp_init();
