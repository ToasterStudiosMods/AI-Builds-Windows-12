/*
 * Aurelian OS - Multiboot Stage-1 Bootloader (32-bit C portion)
 */

#include <stdint.h>
#include <stddef.h>

#define MB_INFO_MEMORY    0x001
#define MB_INFO_MODS      0x008
#define MB_INFO_MMAP      0x040
#define MB_INFO_FRAMEBUFFER 0x400

struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
    uint8_t  color_info[6];
};

struct multiboot_mmap_entry {
    uint32_t size;
    uint64_t base;
    uint64_t length;
    uint32_t type;
} __attribute__((packed));

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

#define COM1 0x3F8

static void serial_init(void)
{
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

static void serial_putc(char c)
{
    while (!(inb(COM1 + 5) & 0x20))
        ;
    outb(COM1, (uint8_t)c);
}

static void serial_puts(const char *s)
{
    for (; *s; s++) {
        if (*s == '\n') serial_putc('\r');
        serial_putc(*s);
    }
}

static void serial_put_u32(uint32_t val)
{
    if (val == 0) { serial_putc('0'); return; }
    char buf[11]; int i = 0;
    while (val > 0 && i < 10) {
        buf[i++] = '0' + (char)(val % 10);
        val /= 10;
    }
    for (int j = i - 1; j >= 0; j--) serial_putc(buf[j]);
}

static void *memset(void *s, int c, size_t n)
{
    uint8_t *p = (uint8_t *)s;
    for (size_t i = 0; i < n; i++) p[i] = (uint8_t)c;
    return s;
}

static void *memcpy(void *dest, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dest;
}

static int memcmp(const void *a, const void *b, size_t n)
{
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;
    for (size_t i = 0; i < n; i++) {
        if (pa[i] != pb[i]) return pa[i] - pb[i];
    }
    return 0;
}

#include "aurelian/boot.h"

#define MAX_MEMORY_REGIONS 128
static struct aurelion_memory_region mem_regions[MAX_MEMORY_REGIONS];
static uint64_t mem_region_count = 0;
static struct aurelion_boot_info boot_info;

typedef struct {
    unsigned char e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} Elf64_Phdr;

static uint64_t pml4[512] __attribute__((aligned(4096)));
static uint64_t pdpt[512] __attribute__((aligned(4096)));
static uint64_t pd_low[512] __attribute__((aligned(4096)));

/* Globals for assembly transition code */
uint32_t stage1_pml4_addr;
uint32_t stage1_boot_info_addr;
uint32_t stage1_kernel_entry;

static void setup_paging(void)
{
    memset(pml4, 0, sizeof(pml4));
    memset(pdpt, 0, sizeof(pdpt));
    memset(pd_low, 0, sizeof(pd_low));

    pml4[0] = (uint32_t)(uintptr_t)&pdpt[0] | 3;
    pml4[510] = (uint32_t)(uintptr_t)&pdpt[0] | 3;
    pdpt[0] = (uint32_t)(uintptr_t)&pd_low[0] | 3;
    for (int i = 0; i < 512; i++) {
        pd_low[i] = (uint64_t)(i * 2 * 1024 * 1024) | 0x83;
    }
}

void multiboot_main(uint32_t mb_magic, uint32_t mb_info_ptr)
{
    struct multiboot_info *mb_info = (struct multiboot_info *)(uintptr_t)mb_info_ptr;

    serial_init();
    serial_puts("Aurelian stage-1 bootloader v0.0.1\n");

    if (mb_magic != 0x2BADB002) {
        serial_puts("ERROR: Bad multiboot magic\n");
        for (;;) __asm__ volatile ("hlt");
    }

    serial_puts("Multiboot OK\n");

    memset(mem_regions, 0, sizeof(mem_regions));
    mem_region_count = 0;

    if ((mb_info->flags & MB_INFO_MMAP) && mb_info->mmap_length > 0) {
        uint32_t mmap_addr = mb_info->mmap_addr;
        uint32_t mmap_end = mmap_addr + mb_info->mmap_length;

        while (mmap_addr + sizeof(struct multiboot_mmap_entry) <= mmap_end &&
               mem_region_count < MAX_MEMORY_REGIONS) {
            struct multiboot_mmap_entry *e =
                (struct multiboot_mmap_entry *)(uintptr_t)mmap_addr;

            if (e->length > 0) {
                uint32_t atype;
                switch (e->type) {
                    case 1: atype = AURELION_MEM_USABLE; break;
                    case 3: atype = AURELION_MEM_ACPI_RECLAIMABLE; break;
                    case 4: atype = AURELION_MEM_ACPI_NVS; break;
                    default: atype = AURELION_MEM_RESERVED; break;
                }
                mem_regions[mem_region_count].base = e->base;
                mem_regions[mem_region_count].length = e->length;
                mem_regions[mem_region_count].type = atype;
                mem_regions[mem_region_count].flags = 0;
                mem_region_count++;
            }
            mmap_addr += e->size + 4;
        }
    } else if (mb_info->flags & MB_INFO_MEMORY) {
        if (mb_info->mem_lower > 0) {
            mem_regions[mem_region_count].base = 0;
            mem_regions[mem_region_count].length = (uint64_t)mb_info->mem_lower * 1024;
            mem_regions[mem_region_count].type = AURELION_MEM_USABLE;
            mem_region_count++;
        }
        if (mb_info->mem_upper > 0) {
            mem_regions[mem_region_count].base = 1024 * 1024;
            mem_regions[mem_region_count].length = (uint64_t)mb_info->mem_upper * 1024;
            mem_regions[mem_region_count].type = AURELION_MEM_USABLE;
            mem_region_count++;
        }
    }

    serial_puts("Memory regions: ");
    serial_put_u32((uint32_t)mem_region_count);
    serial_puts("\n");

    if (!(mb_info->flags & MB_INFO_MODS) || mb_info->mods_count < 1) {
        serial_puts("ERROR: No kernel module.\n");
        for (;;) __asm__ volatile ("hlt");
    }

    uint32_t mod_start = *(uint32_t *)(uintptr_t)mb_info->mods_addr;
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)(uintptr_t)mod_start;

    if (memcmp(ehdr->e_ident, "\177ELF", 4) != 0) {
        serial_puts("ERROR: Not ELF.\n");
        for (;;) __asm__ volatile ("hlt");
    }

    serial_puts("Kernel ELF OK\n");

    uint64_t phys_base = 0xFFFFFFFFFFFFFFFF;
    uint64_t virt_base = 0xFFFFFFFFFFFFFFFF;

    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        Elf64_Phdr *ph = (Elf64_Phdr *)((uint8_t *)ehdr + ehdr->e_phoff +
                                         i * ehdr->e_phentsize);
        if (ph->p_type != 1 || ph->p_memsz == 0) continue;

        memset((void *)(uintptr_t)ph->p_paddr, 0, ph->p_memsz);
        if (ph->p_filesz > 0) {
            memcpy((void *)(uintptr_t)ph->p_paddr,
                   (uint8_t *)ehdr + ph->p_offset, ph->p_filesz);
        }
        if (ph->p_paddr < phys_base) phys_base = ph->p_paddr;
        if (ph->p_vaddr < virt_base) virt_base = ph->p_vaddr;
    }

    serial_puts("Segments loaded\n");

    memset(&boot_info, 0, sizeof(boot_info));
    boot_info.magic = AURELION_BOOT_MAGIC;
    boot_info.abi_version = AURELION_BOOT_ABI_VERSION;
    boot_info.struct_size = sizeof(boot_info);
    boot_info.kernel_physical_base = phys_base;
    boot_info.kernel_virtual_base = virt_base;
    boot_info.memory_region_count = mem_region_count;
    boot_info.memory_regions_physical = (uint64_t)(uintptr_t)&mem_regions;

    setup_paging();

    serial_puts("Entering long mode...\n");

    stage1_pml4_addr = (uint32_t)(uintptr_t)&pml4;
    stage1_boot_info_addr = (uint32_t)(uintptr_t)&boot_info;
    stage1_kernel_entry = (uint32_t)ehdr->e_entry;

    extern void stage1_enter_long_mode(void);
    stage1_enter_long_mode();

    for (;;) __asm__ volatile ("hlt");
}
