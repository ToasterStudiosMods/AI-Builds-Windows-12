/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * heap.c — kernel heap (first-fit free list with coalescing)
 *
 * Until now every buffer in the kernel was a fixed static array, which caps
 * what the system can do: parsers, protocol stacks and task stacks all need
 * storage whose size is only known at run time. This is that missing piece.
 *
 * Layout: one contiguous region carved from the physical arena, divided into
 * blocks. Each block carries an 32-byte header followed by its payload; free
 * neighbours are merged on release to limit fragmentation.
 * ==========================================================================*/

#include "mem.h"
#include "serial.h"
#include "string.h"

#define HEAP_MAGIC 0x48454150u          /* "HEAP" */
#define ALIGN_UP(v, a) (((v) + ((a) - 1)) & ~((uint64_t)(a) - 1))

struct blk {
    uint32_t     magic;
    uint32_t     free;
    uint64_t     size;                  /* payload bytes                */
    struct blk  *next;
    struct blk  *prev;
};

static struct blk *head;
static uint64_t    total_bytes, used_bytes;
static uint32_t    block_count;

/* The allocator is not reentrant, and tasks can be preempted mid-allocation,
 * so serialise with the interrupt flag. Cheap, and correct on a single CPU. */
static inline uint64_t irq_save(void)
{
    uint64_t f;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(f) :: "memory");
    return f;
}
static inline void irq_restore(uint64_t f)
{
    if (f & 0x200) __asm__ volatile ("sti" ::: "memory");
}

void heap_init(uint64_t bytes)
{
    bytes = ALIGN_UP(bytes, 0x1000);
    void *base = phys_alloc(bytes);
    if (!base) { serial_write("[heap] arena exhausted\n"); return; }

    head = (struct blk *)base;
    head->magic = HEAP_MAGIC;
    head->free  = 1;
    head->size  = bytes - sizeof(struct blk);
    head->next  = 0;
    head->prev  = 0;

    total_bytes = head->size;
    used_bytes  = 0;
    block_count = 1;

    serial_write("[heap] ");
    serial_write_u64(total_bytes / 1024);
    serial_write(" KiB at ");
    serial_write_hex((uint64_t)(uintptr_t)base);
    serial_write("\n");
}

void *kmalloc(uint64_t bytes)
{
    if (!head || bytes == 0) return 0;
    bytes = ALIGN_UP(bytes, 16);

    uint64_t f = irq_save();
    for (struct blk *b = head; b; b = b->next) {
        if (!b->free || b->size < bytes) continue;

        /* split when the remainder can still hold a usable block */
        if (b->size >= bytes + sizeof(struct blk) + 32) {
            struct blk *n = (struct blk *)((uint8_t *)(b + 1) + bytes);
            n->magic = HEAP_MAGIC;
            n->free  = 1;
            n->size  = b->size - bytes - sizeof(struct blk);
            n->next  = b->next;
            n->prev  = b;
            if (b->next) b->next->prev = n;
            b->next  = n;
            b->size  = bytes;
            total_bytes -= sizeof(struct blk);
            block_count++;
        }
        b->free = 0;
        used_bytes += b->size;
        irq_restore(f);
        return (void *)(b + 1);
    }
    irq_restore(f);
    return 0;                            /* out of heap */
}

void *kzalloc(uint64_t bytes)
{
    void *p = kmalloc(bytes);
    if (p) memset(p, 0, (size_t)bytes);
    return p;
}

void kfree(void *p)
{
    if (!p) return;
    struct blk *b = ((struct blk *)p) - 1;
    if (b->magic != HEAP_MAGIC || b->free) return;   /* bad or double free */

    uint64_t f = irq_save();
    b->free = 1;
    used_bytes -= b->size;

    /* merge forward, then backward */
    if (b->next && b->next->free) {
        struct blk *n = b->next;
        b->size += n->size + sizeof(struct blk);
        b->next  = n->next;
        if (n->next) n->next->prev = b;
        total_bytes += sizeof(struct blk);
        block_count--;
    }
    if (b->prev && b->prev->free) {
        struct blk *pv = b->prev;
        pv->size += b->size + sizeof(struct blk);
        pv->next  = b->next;
        if (b->next) b->next->prev = pv;
        total_bytes += sizeof(struct blk);
        block_count--;
    }
    irq_restore(f);
}

uint64_t heap_used(void)  { return used_bytes; }
uint64_t heap_total(void) { return total_bytes; }
uint32_t heap_blocks(void){ return block_count; }
