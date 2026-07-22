#ifndef AURELIAN_VMM_H
#define AURELIAN_VMM_H

#include <stdint.h>

#define VMM_PAGE_SIZE    4096
#define VMM_HIGHER_HALF  0xFFFFFFFF80000000ULL  /* Kernel virtual base */

void aurelion_vmm_init(void);
void aurelion_vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags);
void aurelion_vmm_unmap_page(uint64_t virt);
uint64_t aurelion_vmm_get_physical(uint64_t virt);
void aurelion_vmm_load_cr3(void);

#endif
