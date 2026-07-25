/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * include/mem.h — physical arena + kernel heap
 *
 * phys_alloc() hands out page-aligned chunks from the largest free region the
 * loader reported (set up in kernel.c before anything else runs). The heap sits
 * on top of it and provides real dynamic allocation — the prerequisite for
 * anything with a variable shape: task stacks, network buffers, parse trees.
 * ==========================================================================*/

#ifndef AURELIAN_MEM_H
#define AURELIAN_MEM_H

#include <stdint.h>
#include <stddef.h>

/* Page-granular allocation straight out of the arena. Never freed. */
void  *phys_alloc(uint64_t bytes);
uint64_t phys_avail(void);

/* Kernel heap. Safe to call with interrupts on (it masks them internally). */
void   heap_init(uint64_t bytes);
void  *kmalloc(uint64_t bytes);
void   kfree(void *p);
void  *kzalloc(uint64_t bytes);

uint64_t heap_used(void);
uint64_t heap_total(void);
uint32_t heap_blocks(void);

#endif /* AURELIAN_MEM_H */
