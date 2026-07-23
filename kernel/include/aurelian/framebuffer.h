#ifndef AURELIAN_FRAMEBUFFER_H
#define AURELIAN_FRAMEBUFFER_H

#include <stdint.h>

#include "boot.h"

/*
 * The framebuffer renderer is intentionally small and self-contained. It is
 * used before a compositor, font service, or allocator is available, so it
 * only depends on the versioned boot framebuffer ABI.
 */
struct aurelion_framebuffer_surface {
    volatile uint8_t *pixels;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint8_t bytes_per_pixel;
    uint8_t red_mask_size;
    uint8_t red_mask_shift;
    uint8_t green_mask_size;
    uint8_t green_mask_shift;
    uint8_t blue_mask_size;
    uint8_t blue_mask_shift;
};

/* Returns non-zero only for a safe, directly-addressable RGB framebuffer. */
int aurelion_framebuffer_init(struct aurelion_framebuffer_surface *surface,
                              const struct aurelion_framebuffer *framebuffer);

/* Switch a surface to the virtual framebuffer mapping installed by the VMM. */
void aurelion_framebuffer_rebase(struct aurelion_framebuffer_surface *surface,
                                 void *virtual_address);

/* Draw the original Prism UI early-boot canvas. Progress is in the range 0-4. */
void aurelion_framebuffer_draw_boot_splash(
    struct aurelion_framebuffer_surface *surface, uint32_t progress);

#endif
