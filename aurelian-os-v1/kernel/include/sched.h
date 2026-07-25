/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * include/sched.h — preemptive round-robin scheduler (kernel threads)
 * ==========================================================================*/

#ifndef AURELIAN_SCHED_H
#define AURELIAN_SCHED_H

#include <stdint.h>

#define SCHED_MAX_TASKS 8
#define TASK_NAME_MAX   16

enum task_state { TASK_FREE = 0, TASK_READY, TASK_RUNNING, TASK_SLEEPING };

struct task {
    uint64_t rsp;                   /* saved stack pointer when not running */
    uint64_t wake_tick;             /* for TASK_SLEEPING                    */
    uint64_t slices;                /* quantums this task has been given    */
    void    *stack;
    uint32_t stack_size;
    uint8_t  state;
    uint8_t  id;
    char     name[TASK_NAME_MAX];
};

/* Adopt the current execution context as task 0. */
void sched_init(const char *name);

/* Create a kernel thread. Returns its id, or -1. */
int  sched_spawn(const char *name, void (*entry)(void), uint32_t stack_bytes);

/* Called from the timer interrupt with the interrupted stack pointer; returns
 * the stack pointer to resume on (the same one if no switch is due). */
uint64_t sched_tick(uint64_t rsp);

/* Yield the rest of this quantum / sleep for a number of timer ticks. */
void sched_yield(void);
void sched_sleep(uint64_t ticks);

int                 sched_count(void);
const struct task  *sched_task(int i);
int                 sched_current(void);
uint64_t            sched_switches(void);

#endif /* AURELIAN_SCHED_H */
