/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * include/shell.h — Luma Shell (window manager + apps)
 * ==========================================================================*/

#ifndef AURELIAN_SHELL_H
#define AURELIAN_SHELL_H

#include <stdint.h>

/* Run the desktop shell; never returns.
 *   nwp / wp_addr / wp_size — wallpaper boot modules (nwp == 0 -> gradient)
 *   mem_kib                 — usable memory reported by the loader           */
void shell_run(int nwp, const uint64_t *wp_addr, const uint32_t *wp_size,
               uint32_t mem_kib);

#endif /* AURELIAN_SHELL_H */
