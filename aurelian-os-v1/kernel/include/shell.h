/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * include/shell.h — Luma Shell (window manager + apps)
 * ==========================================================================*/

#ifndef AURELIAN_SHELL_H
#define AURELIAN_SHELL_H

#include <stdint.h>

/* Run the desktop shell. Never returns. `n` wallpaper modules are described by
 * the parallel wp_addr/wp_size arrays (n==0 -> gradient background). */
void shell_run(int n, const uint64_t *wp_addr, const uint32_t *wp_size);

#endif /* AURELIAN_SHELL_H */
