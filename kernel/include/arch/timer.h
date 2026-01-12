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
 * @brief Registers a callback handler for the architectural timer interrupt.
 *
 * This function should be used to trigger the scheduler preemption logic.
 *
 * @param handler A pointer to the function to be called on timer expiry.
 */
void arch_timer_set_handler_per_cpu(void (*handler)());

/**
 * @brief Get system uptime in nanoseconds.
 *
 * @return Uptime in nanoseconds.
 */
uint64_t arch_timer_get_uptime_ns();
