/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * include/kernel.h — top-level kernel interface
 * ==========================================================================*/

#ifndef AURELIAN_KERNEL_H
#define AURELIAN_KERNEL_H

#include <stdint.h>

/* Multiboot2 magic + info pointer passed by the bootloader. */
#define MULTIBOOT2_MAGIC 0x36d76289u

/* Kernel identity. */
#define AURELIAN_NAME    "Aurelian OS"
#define AURELIAN_CODENAME "Luma"
#define AURELION_KERNEL  "Aurelion"
#define AURELIAN_VERSION "1.0.0-dev"

/* C entry point, called from boot.S (in 64-bit long mode) with the mbi2
 * info pointer in rdi per the System V AMD64 ABI. */
void kmain(uint64_t mbi2_info);

/* Console helpers backed by the VGA driver. */
void kprint(const char *s);
void kprintln(const char *s);
void kprintf(const char *fmt, ...);

#endif /* AURELIAN_KERNEL_H */
