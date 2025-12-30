#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Stop the current CPU timer.
 *
 * Disables the timer and its interrupt on the current CPU.
 */
void arch_timer_stop();

/**
 * @brief Program a one-shot timer for the current CPU.
 *
 * @param us Timeout in microseconds before the interrupt fires.
 */
void arch_timer_oneshot(size_t us);

/**
 * @brief Program a one-shot timer for the current CPU.
 *
 * @return local_irq Interrupt number targeting the current CPU.
 */
size_t arch_timer_get_local_irq();
uint64_t arch_timer_get_uptime_ns();