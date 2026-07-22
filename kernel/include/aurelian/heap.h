#ifndef AURELIAN_HEAP_H
#define AURELIAN_HEAP_H

#include <stdint.h>
#include <stddef.h>

void  aurelion_heap_init(void *start, size_t size);
void *aurelion_heap_alloc(size_t size);
void  aurelion_heap_free(void *ptr);
size_t aurelion_heap_available(void);

#endif
