/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * include/shell.h — Luma Shell (window manager + apps)
 * ==========================================================================*/

#ifndef AURELIAN_SHELL_H
#define AURELIAN_SHELL_H

#include <stdint.h>

/* Run the desktop shell. Never returns. `wp_*` describe the wallpaper module
 * (have_wp==0 -> gradient background). */
void shell_run(int have_wp, uint64_t wp_addr, uint32_t wp_size);

#endif /* AURELIAN_SHELL_H */
