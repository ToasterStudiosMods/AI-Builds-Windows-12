/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * include/string.h — freestanding string/mem helpers
 * ==========================================================================*/

#ifndef AURELIAN_STRING_H
#define AURELIAN_STRING_H

#include <stddef.h>
#include <stdint.h>

void   *memset(void *dst, int c, size_t n);
void   *memcpy(void *dst, const void *src, size_t n);
size_t  strlen(const char *s);
int     strcmp(const char *a, const char *b);
char   *strcpy(char *dst, const char *src);

/* Convert an unsigned 32-bit value to a base-N string. Returns `buf`. */
char   *uitoa(uint32_t val, char *buf, int base);

#endif /* AURELIAN_STRING_H */
