#include "aurelian/heap.h"
#include "aurelian/serial.h"

/*
 * Simple bump/free-list hybrid kernel heap allocator.
 * Blocks have a header with size and a free flag.
 */

struct heap_block {
    size_t size;        /* Usable size (excluding header) */
    int    is_free;
    struct heap_block *next;
};

#define HEAP_HEADER_SIZE (sizeof(struct heap_block))
#define HEAP_ALIGN       16

static struct heap_block *heap_first = NULL;
static void *heap_brk = NULL;      /* Current heap break (end of managed region) */
static size_t heap_total = 0;     /* Total managed region size */
static size_t heap_used = 0;       /* Currently allocated bytes */

static size_t heap_align_up(size_t val, size_t align)
{
    return (val + align - 1) & ~(align - 1);
}

void aurelion_heap_init(void *start, size_t size)
{
    heap_brk    = (uint8_t *)start + size;
    heap_total  = size;
    heap_used   = 0;

    heap_first = (struct heap_block *)start;
    heap_first->size    = size - HEAP_HEADER_SIZE;
    heap_first->is_free = 1;
    heap_first->next    = NULL;

    aurelion_serial_write("HEAP: initialized at ");
    /* Print start address */
    char buf[21];
    uintptr_t addr = (uintptr_t)start;
    int bi = 0;
    if (addr == 0) {
        aurelion_serial_putc('0');
    } else {
        while (addr > 0 && bi < 20) {
            buf[bi++] = '0' + (char)(addr % 10);
            addr /= 10;
        }
        for (int j = bi - 1; j >= 0; j--) {
            aurelion_serial_putc(buf[j]);
        }
    }
    aurelion_serial_write(", size ");
    addr = size;
    bi = 0;
    if (addr == 0) {
        aurelion_serial_putc('0');
    } else {
        while (addr > 0 && bi < 20) {
            buf[bi++] = '0' + (char)(addr % 10);
            addr /= 10;
        }
        for (int j = bi - 1; j >= 0; j--) {
            aurelion_serial_putc(buf[j]);
        }
    }
    aurelion_serial_write(" bytes.\n");
}

void *aurelion_heap_alloc(size_t size)
{
    if (size == 0) return NULL;

    size = heap_align_up(size, HEAP_ALIGN);

    /* First-fit search */
    struct heap_block *current = heap_first;
    while (current) {
        if (current->is_free && current->size >= size) {
            /* Split block if there's enough space for another block */
            if (current->size >= size + HEAP_HEADER_SIZE + HEAP_ALIGN) {
                struct heap_block *new_block =
                    (struct heap_block *)((uint8_t *)current + HEAP_HEADER_SIZE + size);
                new_block->size    = current->size - size - HEAP_HEADER_SIZE;
                new_block->is_free = 1;
                new_block->next    = current->next;
                current->size      = size;
                current->next      = new_block;
            }

            current->is_free = 0;
            heap_used += size;
            return (void *)((uint8_t *)current + HEAP_HEADER_SIZE);
        }
        current = current->next;
    }

    aurelion_serial_write("HEAP: out of memory!\n");
    return NULL;
}

void aurelion_heap_free(void *ptr)
{
    if (!ptr) return;

    struct heap_block *block =
        (struct heap_block *)((uint8_t *)ptr - HEAP_HEADER_SIZE);

    block->is_free = 1;
    heap_used -= block->size;

    /* Coalesce adjacent free blocks */
    struct heap_block *current = heap_first;
    while (current && current->next) {
        if (current->is_free && current->next->is_free) {
            current->size += HEAP_HEADER_SIZE + current->next->size;
            current->next = current->next->next;
        } else {
            current = current->next;
        }
    }
}

size_t aurelion_heap_available(void)
{
    return heap_total > heap_used ? heap_total - heap_used : 0;
}
