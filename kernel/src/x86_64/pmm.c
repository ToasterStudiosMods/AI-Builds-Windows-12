#include "aurelian/pmm.h"
#include "aurelian/serial.h"

/*
 * Bitmap-based physical memory manager.
 * Each bit in the bitmap represents one 4 KiB physical page.
 * 1 = allocated, 0 = free.
 */

static uint8_t *pmm_bitmap = NULL;
static uint64_t pmm_total_pages = 0;
static uint64_t pmm_free_pages = 0;
static uint64_t pmm_highest_page = 0; /* Highest usable physical page frame */

#define BITMAP_INDEX(frame) ((frame) / 8)
#define BITMAP_OFFSET(frame) ((frame) % 8)
#define BITMAP_SET(frame)    (pmm_bitmap[BITMAP_INDEX(frame)] |=  (1u << BITMAP_OFFSET(frame)))
#define BITMAP_CLEAR(frame)  (pmm_bitmap[BITMAP_INDEX(frame)] &= ~(1u << BITMAP_OFFSET(frame)))
#define BITMAP_TEST(frame)   (pmm_bitmap[BITMAP_INDEX(frame)] &   (1u << BITMAP_OFFSET(frame)))

static void pmm_mark_region(uint64_t base, uint64_t length, int allocated)
{
    uint64_t start_frame = base / PMM_PAGE_SIZE;
    uint64_t end_frame   = (base + length) / PMM_PAGE_SIZE;

    for (uint64_t f = start_frame; f < end_frame && f < pmm_total_pages; f++) {
        if (allocated) {
            BITMAP_SET(f);
        } else {
            BITMAP_CLEAR(f);
        }
    }
}

void aurelion_pmm_init(const struct aurelion_boot_info *boot_info)
{
    if (!boot_info || boot_info->memory_region_count == 0) {
        aurelion_serial_write("PMM: no memory map provided, cannot init.\n");
        return;
    }

    const struct aurelion_memory_region *regions =
        (const struct aurelion_memory_region *)(uintptr_t)boot_info->memory_regions_physical;

    /* Find the highest usable physical address */
    uint64_t max_phys = 0;
    for (uint64_t i = 0; i < boot_info->memory_region_count; i++) {
        uint64_t end = regions[i].base + regions[i].length;
        if (end > max_phys) {
            max_phys = end;
        }
    }

    pmm_total_pages  = max_phys / PMM_PAGE_SIZE;
    pmm_highest_page = pmm_total_pages;

    /* Mark all pages as allocated initially */
    pmm_free_pages = 0;

    /* Place bitmap just after kernel (use a simple heuristic) */
    /* For now, place it at 2 MiB physical - will be refined with VMM */
    uint64_t bitmap_size = pmm_total_pages / 8;
    if (pmm_total_pages % 8 != 0) {
        bitmap_size++;
    }
    /* Use the first 2 MiB region for the bitmap, which is below kernel load at 1 MiB.
     * Actually kernel is at 1 MiB so place bitmap after kernel BSS end.
     * For simplicity, place at physical 3 MiB for now. */
    pmm_bitmap = (uint8_t *)(uintptr_t)(3 * 1024 * 1024);

    /* Zero and mark all as allocated */
    for (uint64_t i = 0; i < bitmap_size; i++) {
        pmm_bitmap[i] = 0xFF;
    }

    /* Mark usable regions as free */
    for (uint64_t i = 0; i < boot_info->memory_region_count; i++) {
        if (regions[i].type == AURELION_MEM_USABLE) {
            pmm_mark_region(regions[i].base, regions[i].length, 0);
        }
    }

    /* Count free pages */
    pmm_free_pages = 0;
    for (uint64_t i = 0; i < pmm_total_pages; i++) {
        if (!BITMAP_TEST(i)) {
            pmm_free_pages++;
        }
    }

    /* Mark bitmap pages themselves as allocated */
    uint64_t bitmap_start = (3 * 1024 * 1024) / PMM_PAGE_SIZE;
    uint64_t bitmap_end   = bitmap_start + (bitmap_size + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;
    for (uint64_t f = bitmap_start; f < bitmap_end && f < pmm_total_pages; f++) {
        BITMAP_SET(f);
    }

    /* Mark first page (null pointer guard) as allocated */
    BITMAP_SET(0);

    aurelion_serial_write("PMM: initialized, ");
    /* Print available pages */
    char buf[21];
    uint64_t avail = pmm_free_pages;
    int bi = 0;
    if (avail == 0) {
        aurelion_serial_putc('0');
    } else {
        while (avail > 0 && bi < 20) {
            buf[bi++] = '0' + (char)(avail % 10);
            avail /= 10;
        }
        for (int j = bi - 1; j >= 0; j--) {
            aurelion_serial_putc(buf[j]);
        }
    }
    aurelion_serial_write(" free pages (");
    /* Print total pages */
    avail = pmm_total_pages;
    bi = 0;
    if (avail == 0) {
        aurelion_serial_putc('0');
    } else {
        while (avail > 0 && bi < 20) {
            buf[bi++] = '0' + (char)(avail % 10);
            avail /= 10;
        }
        for (int j = bi - 1; j >= 0; j--) {
            aurelion_serial_putc(buf[j]);
        }
    }
    aurelion_serial_write(" total).\n");
}

void *aurelion_pmm_alloc_page(void)
{
    for (uint64_t i = 0; i < pmm_total_pages; i++) {
        if (!BITMAP_TEST(i)) {
            BITMAP_SET(i);
            pmm_free_pages--;
            return (void *)(uintptr_t)(i * PMM_PAGE_SIZE);
        }
    }
    aurelion_serial_write("PMM: out of memory!\n");
    return NULL;
}

void aurelion_pmm_free_page(void *page)
{
    uint64_t frame = (uint64_t)(uintptr_t)page / PMM_PAGE_SIZE;
    if (frame < pmm_total_pages && BITMAP_TEST(frame)) {
        BITMAP_CLEAR(frame);
        pmm_free_pages++;
    }
}

uint64_t aurelion_pmm_available_pages(void)
{
    return pmm_free_pages;
}

void aurelion_pmm_dump(void)
{
    aurelion_serial_write("PMM: bitmap at ");
    /* Simple hex print of bitmap pointer */
    /* TODO: implement hex print */
    aurelion_serial_write(" (placeholder)\n");
}
