/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * sched.c — preemptive round-robin scheduler for kernel threads
 *
 * How the switch works: every interrupt entry in isr.S already pushes a full
 * register frame, so a task's entire CPU state is sitting on its own stack.
 * interrupt_common() hands the interrupted stack pointer to sched_tick(), which
 * stores it in the outgoing task and returns the incoming task's pointer. isr.S
 * loads that into RSP before popping, so the pops and the final iretq restore a
 * different thread. The switch is therefore just "swap the stack".
 *
 * A freshly spawned task has no saved state, so we forge a frame on its stack
 * that looks exactly like an interrupt frame — right down to RFLAGS with the
 * interrupt flag set — and the first switch "returns" into its entry point.
 * ==========================================================================*/

#include "sched.h"
#include "mem.h"
#include "timer.h"
#include "serial.h"
#include "string.h"

static struct task tasks[SCHED_MAX_TASKS];
static int      cur;                  /* index of the running task */
static int      ready;                /* scheduler armed?          */
static uint64_t switches;

static void set_name(char *dst, const char *src)
{
    int i = 0;
    while (src[i] && i < TASK_NAME_MAX - 1) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

void sched_init(const char *name)
{
    memset(tasks, 0, sizeof(tasks));
    tasks[0].state = TASK_RUNNING;
    tasks[0].id    = 0;
    set_name(tasks[0].name, name);
    cur   = 0;
    ready = 1;
    serial_write("[sched] task 0 '"); serial_write(name); serial_write("' adopted\n");
}

int sched_spawn(const char *name, void (*entry)(void), uint32_t stack_bytes)
{
    int slot = -1;
    for (int i = 1; i < SCHED_MAX_TASKS; i++)
        if (tasks[i].state == TASK_FREE) { slot = i; break; }
    if (slot < 0) return -1;

    if (stack_bytes < 8192) stack_bytes = 8192;
    uint8_t *stk = (uint8_t *)kmalloc(stack_bytes);
    if (!stk) return -1;

    /* Build the frame isr.S expects to pop, from the top of the stack down:
     *   ss, rsp, rflags, cs, rip   (what iretq consumes)
     *   error code, vector         (discarded by the addq $16)
     *   r15..rax                   (15 general purpose registers)
     * 16-byte align the iretq frame so the ABI holds inside the task. */
    uint64_t *sp = (uint64_t *)(stk + stack_bytes);
    sp = (uint64_t *)((uint64_t)sp & ~0xFull);
    /* Capture the top before pushing: this is what RSP must hold once iretq has
     * consumed the frame. Reading sp in the same expression that decrements it
     * would be undefined behaviour, and the value would be wrong besides. */
    uint64_t stack_top = (uint64_t)(uintptr_t)sp;

    *(--sp) = 0x10;                            /* ss     — kernel data      */
    *(--sp) = stack_top;                       /* rsp    — task's own stack */
    *(--sp) = 0x202;                           /* rflags — IF set           */
    *(--sp) = 0x08;                            /* cs     — kernel code      */
    *(--sp) = (uint64_t)(uintptr_t)entry;      /* rip    — entry point      */
    *(--sp) = 0;                               /* error code                */
    *(--sp) = 32;                              /* vector (timer)            */
    for (int i = 0; i < 15; i++) *(--sp) = 0;  /* r15..rax                  */

    tasks[slot].rsp        = (uint64_t)(uintptr_t)sp;
    tasks[slot].stack      = stk;
    tasks[slot].stack_size = stack_bytes;
    tasks[slot].state      = TASK_READY;
    tasks[slot].slices     = 0;
    tasks[slot].id         = (uint8_t)slot;
    set_name(tasks[slot].name, name);

    serial_write("[sched] spawned '"); serial_write(name); serial_write("'\n");
    return slot;
}

/* Pick the next runnable task after `from`, or -1 if nobody else can run. */
static int next_runnable(int from)
{
    uint64_t now = timer_ticks();
    for (int step = 1; step <= SCHED_MAX_TASKS; step++) {
        int i = (from + step) % SCHED_MAX_TASKS;
        if (tasks[i].state == TASK_SLEEPING && now >= tasks[i].wake_tick)
            tasks[i].state = TASK_READY;
        if (tasks[i].state == TASK_READY || tasks[i].state == TASK_RUNNING)
            return i;
    }
    return -1;
}

uint64_t sched_tick(uint64_t rsp)
{
    if (!ready) return rsp;

    tasks[cur].slices++;

    int nxt = next_runnable(cur);
    if (nxt < 0 || nxt == cur) return rsp;      /* nothing else to run */

    tasks[cur].rsp = rsp;
    if (tasks[cur].state == TASK_RUNNING) tasks[cur].state = TASK_READY;

    cur = nxt;
    tasks[cur].state = TASK_RUNNING;
    switches++;
    return tasks[cur].rsp;
}

void sched_yield(void)
{
    /* Give up the rest of the quantum by taking a timer-vector interrupt: the
     * handler runs the normal switch path, so no separate mechanism is needed. */
    __asm__ volatile ("int $32");
}

void sched_sleep(uint64_t ticks)
{
    if (!ready) return;
    tasks[cur].wake_tick = timer_ticks() + ticks;
    tasks[cur].state     = TASK_SLEEPING;
    sched_yield();
}

int  sched_count(void)
{
    int n = 0;
    for (int i = 0; i < SCHED_MAX_TASKS; i++) if (tasks[i].state != TASK_FREE) n++;
    return n;
}

const struct task *sched_task(int i)
{ return (i >= 0 && i < SCHED_MAX_TASKS) ? &tasks[i] : 0; }

int      sched_current(void)  { return cur; }
uint64_t sched_switches(void) { return switches; }
