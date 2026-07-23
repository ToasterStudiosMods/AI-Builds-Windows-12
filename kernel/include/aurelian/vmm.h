#ifndef AURELIAN_VMM_H
#define AURELIAN_VMM_H

#include <stdint.h>

#include "boot.h"

#define VMM_PAGE_SIZE    4096
#define VMM_HIGHER_HALF  0xFFFFFFFF80000000ULL  /* Kernel virtual base */
#define VMM_FRAMEBUFFER_BASE 0xFFFF900000000000ULL
#define VMM_PAGE_WRITABLE 0x002ULL

/* Maps an optional boot framebuffer before loading the new CR3. */
void *aurelion_vmm_init(const struct aurelion_framebuffer *framebuffer);
void aurelion_vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags);
void aurelion_vmm_unmap_page(uint64_t virt);
uint64_t aurelion_vmm_get_physical(uint64_t virt);
void aurelion_vmm_load_cr3(void);
void *aurelion_vmm_map_framebuffer(
    const struct aurelion_framebuffer *framebuffer);

#endif
