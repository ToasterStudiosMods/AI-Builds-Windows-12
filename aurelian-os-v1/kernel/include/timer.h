/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * include/timer.h — PIT timer (A2, ChatGPT)
 * ==========================================================================*/

#ifndef AURELIAN_TIMER_H
#define AURELIAN_TIMER_H

#include <stdint.h>

void     timer_init(uint32_t hz);
uint64_t timer_ticks(void);

#endif /* AURELIAN_TIMER_H */
