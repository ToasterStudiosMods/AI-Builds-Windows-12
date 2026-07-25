/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * pit.c — 8253/8254 PIT timer driver (A2, contributed by ChatGPT)
 * Reviewed + integrated as-is; clamps the divisor to the 16-bit range.
 * ==========================================================================*/

#include <stdint.h>

#include "timer.h"
#include "io.h"
#include "interrupts.h"

#define PIT_COMMAND_PORT   0x43
#define PIT_CHANNEL0_PORT  0x40
#define PIT_BASE_FREQUENCY 1193182U

static volatile uint64_t pit_ticks = 0;

static void pit_irq_handler(void)
{
    pit_ticks++;
}

void timer_init(uint32_t hz)
{
    uint32_t divisor;

    if (hz == 0)
        hz = 100;

    divisor = PIT_BASE_FREQUENCY / hz;

    if (divisor == 0)
        divisor = 1;
    if (divisor > 0xFFFF)
        divisor = 0xFFFF;          /* keep the reload value in the 16-bit range */

    outb(PIT_COMMAND_PORT, 0x36);
    outb(PIT_CHANNEL0_PORT, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0_PORT, (uint8_t)((divisor >> 8) & 0xFF));

    irq_install(0, pit_irq_handler);
    irq_unmask(0);
}

uint64_t timer_ticks(void)
{
    return pit_ticks;
}
