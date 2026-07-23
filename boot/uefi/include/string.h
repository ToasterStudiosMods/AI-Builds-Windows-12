#ifndef AURELIAN_UEFI_STRING_H
#define AURELIAN_UEFI_STRING_H

#include <stddef.h>

/* Minimal freestanding subset used by the UEFI loader. The loader links
 * with -nostdlib, so it cannot rely on the mingw runtime's string.c. */
void *memset(void *dest, int value, size_t count);
void *memcpy(void *dest, const void *src, size_t count);
int memcmp(const void *a, const void *b, size_t count);

#endif
