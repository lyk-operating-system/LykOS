#pragma once

#include "proc/proc.h"
#include "proc/smp.h"
#include "proc/thread.h"

thread_t *sched_get_curr_thread();
uint32_t sched_get_curr_cpuid();

void sched_enqueue(thread_t *t);

void sched_preemt();
void sched_yield(thread_status_t status);
