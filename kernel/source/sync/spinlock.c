#include "sync/spinlock.h"

#include "arch/lcpu.h"
#include "panic.h"

void spinlock_acquire(volatile spinlock_t *slock)
{
    volatile size_t deadlock_cnt = 0;

    while (true)
    {
        if (!__atomic_test_and_set(&slock->lock, __ATOMIC_ACQUIRE))
        {
            slock->prev_int_state = arch_lcpu_int_enabled();
                arch_lcpu_int_mask();
            return;
        }

        while (__atomic_load_n(&slock->lock, __ATOMIC_RELAXED))
        {
            arch_lcpu_relax();
            if (deadlock_cnt++ >= 3'000'000)
                panic("Deadlock occured. Return addr: %llx", __builtin_return_address(0));
        }
    }
}

void spinlock_release(volatile spinlock_t *slock)
{
    bool prev_int_state = slock->prev_int_state;
    __atomic_clear(&slock->lock, __ATOMIC_RELEASE);
    if (prev_int_state)
        arch_lcpu_int_unmask();
}

void spinlock_primitive_acquire(volatile spinlock_t *slock)
{
    while (true)
    {
        if (!__atomic_test_and_set(&slock->lock, __ATOMIC_ACQUIRE))
            return;

        while (__atomic_load_n(&slock->lock, __ATOMIC_RELAXED))
            arch_lcpu_relax();
    }
}

void spinlock_primitive_release(volatile spinlock_t *slock)
{
    __atomic_clear(&slock->lock, __ATOMIC_RELEASE);
}
