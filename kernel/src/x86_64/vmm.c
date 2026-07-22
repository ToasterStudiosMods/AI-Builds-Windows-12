#include "aurelian/vmm.h"
#include "aurelian/pmm.h"
#include "aurelian/serial.h"

/*
 * Virtual memory manager using 4-level paging (PML4 -> PDPT -> PD -> PT).
 * Implements higher-half kernel mapping.
 */

#define PTE_PRESENT     (1ULL << 0)
#define PTE_WRITABLE    (1ULL << 1)
#define PTE_USER        (1ULL << 2)
#define PTE_NO_EXECUTE  (1ULL << 63)

static uint64_t pml4[512] __attribute__((aligned(4096)));
static uint64_t pdpt[512] __attribute__((aligned(4096)));
static uint64_t pd[512]  __attribute__((aligned(4096)));
static uint64_t pt_low[512]  __attribute__((aligned(4096)));  /* First 2 MiB identity map */
static uint64_t pt_kernel[512] __attribute__((aligned(4096))); /* Kernel pages at higher half */

static inline uint64_t *vmm_get_next_level(uint64_t *current, uint64_t index, int allocate)
{
    if (current[index] & PTE_PRESENT) {
        return (uint64_t *)(uintptr_t)(current[index] & 0x000FFFFFFFFFF000ULL);
    }
    if (allocate) {
        void *page = aurelion_pmm_alloc_page();
        if (!page) return NULL;
        for (int i = 0; i < 512; i++) {
            ((uint64_t *)page)[i] = 0;
        }
        current[index] = (uint64_t)(uintptr_t)page | PTE_PRESENT | PTE_WRITABLE;
        return (uint64_t *)page;
    }
    return NULL;
}

void aurelion_vmm_init(void)
{
    /* Zero all page tables */
    for (int i = 0; i < 512; i++) {
        pml4[i] = 0;
        pdpt[i] = 0;
        pd[i]   = 0;
        pt_low[i]   = 0;
        pt_kernel[i] = 0;
    }

    /*
     * Map the first 2 MiB (identity map) for boot transition.
     * PML4[0] -> PDPT[0] -> PD[0] -> PT_low (4 KiB pages for first 2 MiB)
     */
    pml4[0] = (uint64_t)(uintptr_t)&pdpt[0] | PTE_PRESENT | PTE_WRITABLE;
    pdpt[0] = (uint64_t)(uintptr_t)&pd[0] | PTE_PRESENT | PTE_WRITABLE;
    pd[0]   = (uint64_t)(uintptr_t)&pt_low[0] | PTE_PRESENT | PTE_WRITABLE;

    for (int i = 0; i < 512; i++) {
        pt_low[i] = (uint64_t)(i * 4096) | PTE_PRESENT | PTE_WRITABLE;
    }

    /*
     * Higher-half kernel mapping.
     * Virtual 0xFFFFFFFF80000000 maps to physical 0x00000000
     * PML4[510] (index for 0xFFFF8000...) -> PDPT[0] -> PD[0] -> same PT
     */
    /* Use a separate PDPT for higher half to avoid aliasing */
    pml4[510] = (uint64_t)(uintptr_t)&pdpt[0] | PTE_PRESENT | PTE_WRITABLE;

    /* Load CR3 */
    aurelion_vmm_load_cr3();

    aurelion_serial_write("VMM: 4-level paging initialized (identity + higher-half).\n");
}

void aurelion_vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags)
{
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    uint64_t *p = vmm_get_next_level(pml4, pml4_idx, 1);
    if (!p) return;
    p = vmm_get_next_level(p, pdpt_idx, 1);
    if (!p) return;
    p = vmm_get_next_level(p, pd_idx, 1);
    if (!p) return;

    p[pt_idx] = (phys & 0x000FFFFFFFFFF000ULL) | (flags & 0xFFFULL) | PTE_PRESENT;
}

void aurelion_vmm_unmap_page(uint64_t virt)
{
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    uint64_t *p = vmm_get_next_level(pml4, pml4_idx, 0);
    if (!p) return;
    p = vmm_get_next_level(p, pdpt_idx, 0);
    if (!p) return;
    p = vmm_get_next_level(p, pd_idx, 0);
    if (!p) return;

    p[pt_idx] = 0;
    /* Invalidate TLB entry */
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

uint64_t aurelion_vmm_get_physical(uint64_t virt)
{
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    uint64_t *p = vmm_get_next_level(pml4, pml4_idx, 0);
    if (!p) return 0;
    p = vmm_get_next_level(p, pdpt_idx, 0);
    if (!p) return 0;
    p = vmm_get_next_level(p, pd_idx, 0);
    if (!p) return 0;

    if (!(p[pt_idx] & PTE_PRESENT)) return 0;
    return p[pt_idx] & 0x000FFFFFFFFFF000ULL;
}

void aurelion_vmm_load_cr3(void)
{
    __asm__ volatile (
        "mov %0, %%cr3"
        : : "r"((uint64_t)(uintptr_t)&pml4)
        : "memory"
    );
}
