#include "sys/reaper.h"

#include "mm/heap.h"
#include "panic.h"
#include "sys/proc.h"
#include "sys/sched.h"
#include "sys/thread.h"
#include "uapi/errno.h"
#include "utils/container_of.h"

static list_t threads_queue = LIST_INIT;
static spinlock_t slock_threads = SPINLOCK_INIT;

void reaper_enqueue_thread(thread_t *t)
{
    spinlock_acquire(&slock_threads);

    list_append(&threads_queue, &t->sched_thread_list_node);

    spinlock_release(&slock_threads);
}

static void reaper_main()
{
    while (true)
    {
        spinlock_acquire(&slock_threads);

        FOREACH(n, threads_queue)
        {
            thread_t *t = container_of(n, thread_t, sched_thread_list_node);
            if (t->ref_count != 0)
                continue;

            spinlock_acquire(&t->owner->slock);
            if (t->ref_count != 0)
            {
                spinlock_release(&t->owner->slock);
                continue;
            }
            list_remove(&t->owner->threads, &t->proc_thread_list_node);
            spinlock_release(&t->owner->slock);

            thread_free(t);
        }

        spinlock_release(&slock_threads);
    }
}

void reaper_init()
{
    proc_t *reaper_proc;
    thread_t *reaper_thread;

    if (proc_create_kernel("Reaper", &reaper_proc) != EOK)
        panic("Could not initialize the reaper!");

    if (thread_create_kernel(reaper_proc->as, (uintptr_t)&reaper_main, 4096, &reaper_thread) != EOK)
        panic("Could not initialize the reaper!");
    reaper_thread->owner = reaper_proc;
    list_append(&reaper_proc->threads, &reaper_thread->proc_thread_list_node);

    sched_enqueue(reaper_thread);
}
