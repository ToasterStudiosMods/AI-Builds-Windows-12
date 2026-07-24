/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * input.c — lock-free-ish single-producer/single-consumer event ring buffer
 *
 * Producer: IRQ handlers (keyboard/mouse) call input_push().
 * Consumer: the main UI loop calls input_poll().
 * ==========================================================================*/

#include "input.h"

#define INPUT_QUEUE_SIZE 128   /* power of two keeps the modulo cheap */

static struct input_event queue[INPUT_QUEUE_SIZE];
static volatile uint32_t   head;   /* next write slot (producer)  */
static volatile uint32_t   tail;   /* next read slot  (consumer)  */

void input_push(const struct input_event *e)
{
    uint32_t next = (head + 1u) & (INPUT_QUEUE_SIZE - 1u);
    if (next == tail)
        return;                    /* full — drop the event */
    queue[head] = *e;
    head = next;
}

int input_poll(struct input_event *out)
{
    if (tail == head)
        return 0;                  /* empty */
    *out = queue[tail];
    tail = (tail + 1u) & (INPUT_QUEUE_SIZE - 1u);
    return 1;
}
