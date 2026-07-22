#ifndef AURELIAN_PMM_H
#define AURELIAN_PMM_H

#include <stdint.h>
#include <stddef.h>
#include "boot.h"

#define PMM_PAGE_SIZE  4096

void  aurelion_pmm_init(const struct aurelion_boot_info *boot_info);
void *aurelion_pmm_alloc_page(void);
void  aurelion_pmm_free_page(void *page);
uint64_t aurelion_pmm_available_pages(void);
void  aurelion_pmm_dump(void);

#endif
