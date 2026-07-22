#ifndef AURELIAN_BOOT_H
#define AURELIAN_BOOT_H

#include <stdint.h>

#define AURELION_BOOT_MAGIC 0x414F53524C4E4255ULL /* "AOSRLNBU" marker */
#define AURELION_BOOT_ABI_VERSION 1u

#define AURELION_MEM_USABLE 1u
#define AURELION_MEM_RESERVED 2u
#define AURELION_MEM_ACPI_RECLAIMABLE 3u
#define AURELION_MEM_ACPI_NVS 4u
#define AURELION_MEM_BAD 5u

struct aurelion_memory_region {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t flags;
};

struct aurelion_framebuffer {
    uint64_t address;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    uint32_t red_mask_size;
    uint32_t red_mask_shift;
    uint32_t green_mask_size;
    uint32_t green_mask_shift;
    uint32_t blue_mask_size;
    uint32_t blue_mask_shift;
};

struct aurelion_boot_info {
    uint64_t magic;
    uint32_t abi_version;
    uint32_t struct_size;
    uint64_t kernel_physical_base;
    uint64_t kernel_virtual_base;
    uint64_t rsdp_physical;
    uint64_t memory_region_count;
    uint64_t memory_regions_physical;
    struct aurelion_framebuffer framebuffer;
    uint64_t initrd_physical;
    uint64_t initrd_size;
    uint64_t command_line_physical;
};

_Static_assert(sizeof(struct aurelion_memory_region) == 24, "memory region ABI size changed");
_Static_assert(sizeof(struct aurelion_framebuffer) == 48, "framebuffer ABI size changed");
_Static_assert(sizeof(struct aurelion_boot_info) == 128, "boot info ABI size changed");

#endif
