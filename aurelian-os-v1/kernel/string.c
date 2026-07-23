/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * string.c — freestanding string/mem helpers
 * ==========================================================================*/

#include "string.h"

void *memset(void *dst, int c, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    while (n--) *d++ = (unsigned char)c;
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n)
{
    unsigned char       *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dst;
}

size_t strlen(const char *s)
{
    const char *p = s;
    while (*p) p++;
    return (size_t)(p - s);
}

int strcmp(const char *a, const char *b)
{
    while (*a && (*a == *b)) { a++; b++; }
    return (int)((unsigned char)*a - (unsigned char)*b);
}

char *strcpy(char *dst, const char *src)
{
    char *d = dst;
    while ((*d++ = *src++)) { /* copy */ }
    return dst;
}

/* Convert an unsigned int to a NUL-terminated string in the given base
 * (2..16). Caller must ensure `buf` is large enough (33 bytes for base 2). */
char *uitoa(uint32_t val, char *buf, int base)
{
    const char *digits = "0123456789abcdef";
    char        tmp[33];
    int         i = 0;

    if (base < 2 || base > 16) base = 10;
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return buf;
    }
    while (val && i < (int)sizeof(tmp)) {
        tmp[i++] = digits[val % (uint32_t)base];
        val /= (uint32_t)base;
    }
    int j = 0;
    while (i) buf[j++] = tmp[--i];
    buf[j] = '\0';
    return buf;
}
