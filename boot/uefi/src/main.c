/*
 * Aurelian OS UEFI Bootloader
 *
 * This is a standalone UEFI application (BOOTX64.EFI) that:
 * 1. Loads aurelion.elf from the EFI System Partition
 * 2. Parses the ELF64 program headers
 * 3. Allocates pages for each loadable segment
 * 4. Copies kernel segments into memory and zeroes BSS
 * 5. Gathers the UEFI memory map
 * 6. Locates the ACPI RSDP from the configuration table
 * 7. Queries the GOP framebuffer details
 * 8. Builds struct aurelion_boot_info
 * 9. Exits UEFI boot services
 * 10. Jumps to the kernel entry point
 */

#include <efi/efi.h>
#include <efi/efiprotocols.h>
#include <string.h>

/* Include the kernel's boot ABI definitions */
#include "../../../kernel/include/aurelian/boot.h"

/* GUIDs */
static EFI_GUID gEfiLoadedImageProtocolGuid = {
    0x5B1B31A1, 0x9562, 0x11D2,
    { 0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B }
};

static EFI_GUID gEfiSimpleFileSystemProtocolGuid = {
    0x0964E5B22, 0x6149, 0x11D2,
    { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B }
};

static EFI_GUID gEfiGraphicsOutputProtocolGuid = {
    0x9042A9DE, 0x23DC, 0x4A38,
    { 0x96, 0xFB, 0x7A, 0xDE, 0xD0, 0x80, 0x51, 0x6A }
};

static EFI_GUID gEfiAcpi20TableGuid = {
    0x8868E871, 0xE4F1, 0x11D3,
    { 0xBC, 0x22, 0x00, 0x80, 0xC7, 0x3C, 0x88, 0x81 }
};

static EFI_GUID gEfiAcpi10TableGuid = {
    0xEB9D2D31, 0x2D88, 0x11D3,
    { 0x9A, 0x16, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D }
};

static EFI_GUID gEfiFileInfoGuid = {
    0x0954E92A, 0x6228, 0x11D2,
    { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B }
};

/* ELF64 structures */
#define EI_NIDENT 16

typedef struct {
    unsigned char e_ident[EI_NIDENT];
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

#define PT_LOAD    1
#define PT_NULL    0
#define ELF_MAGIC  0x464C457F /* "\x7fELF" */

/* Global UEFI system table pointer */
static EFI_SYSTEM_TABLE *gST = NULL;

/* Simple serial-like console output via ConOut */
static void print_str(const char *s)
{
    if (!gST || !gST->ConOut) return;
    for (int i = 0; s[i]; i++) {
        CHAR16 c = (CHAR16)(uint8_t)s[i];
        gST->ConOut->OutputString(gST->ConOut, &c);
    }
}

static void print_hex(uint64_t val)
{
    char buf[17];
    static const char hex[] = "0123456789ABCDEF";
    for (int i = 15; i >= 0; i--) {
        buf[15 - i] = hex[(val >> (i * 4)) & 0xF];
    }
    buf[16] = '\0';
    /* Skip leading zeros */
    int start = 0;
    while (start < 15 && buf[start] == '0') start++;
    if (start > 15) start = 15;
    print_str("0x");
    print_str(buf + start);
}

static void print_str2(const char *s, int len);

static uint32_t mask_shift(uint32_t mask)
{
    uint32_t shift = 0;
    while (mask != 0 && (mask & 1U) == 0) {
        mask >>= 1;
        shift++;
    }
    return shift;
}

static uint32_t mask_size(uint32_t mask)
{
    uint32_t size = 0;
    while (mask != 0) {
        size += mask & 1U;
        mask >>= 1;
    }
    return size;
}

static void set_framebuffer_format(
    struct aurelion_framebuffer *framebuffer,
    const EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *mode)
{
    if (mode->PixelFormat == PixelRedGreenBlueReserved8BitPerColor) {
        framebuffer->red_mask_size = 8;
        framebuffer->red_mask_shift = 0;
        framebuffer->green_mask_size = 8;
        framebuffer->green_mask_shift = 8;
        framebuffer->blue_mask_size = 8;
        framebuffer->blue_mask_shift = 16;
    } else if (mode->PixelFormat == PixelBlueGreenRedReserved8BitPerColor) {
        framebuffer->red_mask_size = 8;
        framebuffer->red_mask_shift = 16;
        framebuffer->green_mask_size = 8;
        framebuffer->green_mask_shift = 8;
        framebuffer->blue_mask_size = 8;
        framebuffer->blue_mask_shift = 0;
    } else if (mode->PixelFormat == PixelBitMask) {
        framebuffer->red_mask_size = mask_size(mode->PixelInformation.RedMask);
        framebuffer->red_mask_shift = mask_shift(mode->PixelInformation.RedMask);
        framebuffer->green_mask_size = mask_size(mode->PixelInformation.GreenMask);
        framebuffer->green_mask_shift = mask_shift(mode->PixelInformation.GreenMask);
        framebuffer->blue_mask_size = mask_size(mode->PixelInformation.BlueMask);
        framebuffer->blue_mask_shift = mask_shift(mode->PixelInformation.BlueMask);
    }
}

static void print_u64(uint64_t val)
{
    char buf[21];
    if (val == 0) {
        print_str("0");
        return;
    }
    int i = 0;
    while (val > 0) {
        buf[i++] = '0' + (char)(val % 10);
        val /= 10;
    }
    for (int j = i - 1; j >= 0; j--) {
        print_str2(buf + j, 1);
    }
}

/* Helper to print a single char */
static void print_str2(const char *s, int len)
{
    if (!gST || !gST->ConOut) return;
    for (int i = 0; i < len && s[i]; i++) {
        CHAR16 c = (CHAR16)(uint8_t)s[i];
        gST->ConOut->OutputString(gST->ConOut, &c);
    }
}

/* Read a file from the volume into allocated memory */
static EFI_STATUS read_file(EFI_FILE_PROTOCOL *root, const CHAR16 *filename,
                             void **buffer, UINTN *size)
{
    EFI_FILE_PROTOCOL *file;
    EFI_STATUS status = root->Open(root, &file, (CHAR16 *)filename,
                                   EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) {
        print_str("ERROR: Cannot open ");
        /* Print filename as bytes */
        for (int i = 0; filename[i]; i++) {
            CHAR16 c = filename[i];
            gST->ConOut->OutputString(gST->ConOut, &c);
        }
        print_str("\n");
        return status;
    }

    /* Get file size by reading info */
    UINTN info_size = 0;
    file->GetInfo(file, &gEfiFileInfoGuid, &info_size, NULL);
    if (info_size == 0) {
        file->Close(file);
        return EFI_DEVICE_ERROR;
    }

    void *info = NULL;
    gST->BootServices->AllocatePool(EFI_LOADER_DATA, info_size, &info);
    if (!info) {
        file->Close(file);
        return EFI_OUT_OF_RESOURCES;
    }

    status = file->GetInfo(file, &gEfiFileInfoGuid, &info_size, info);
    if (EFI_ERROR(status)) {
        gST->BootServices->FreePool(info);
        file->Close(file);
        return status;
    }

    UINTN file_size = *(UINTN *)((uint8_t *)info + 8); /* Offset to FileSize in EFI_FILE_INFO */
    gST->BootServices->FreePool(info);

    /* Allocate buffer for file contents */
    status = gST->BootServices->AllocatePool(EFI_LOADER_DATA, file_size, buffer);
    if (EFI_ERROR(status)) {
        file->Close(file);
        return EFI_OUT_OF_RESOURCES;
    }

    *size = file_size;
    UINTN read_size = file_size;
    status = file->Read(file, &read_size, *buffer);
    file->Close(file);

    if (EFI_ERROR(status) || read_size != file_size) {
        print_str("ERROR: Failed to read kernel file\n");
        return EFI_DEVICE_ERROR;
    }

    return EFI_SUCCESS;
}

/* Memory map storage for passing to kernel */
#define MAX_MEMORY_REGIONS 256
static struct aurelion_memory_region memory_regions[MAX_MEMORY_REGIONS];
static uint64_t memory_region_count = 0;

/* Convert UEFI memory map to Aurelian format */
static void convert_memory_map(void *uefi_map, UINTN map_size, UINTN desc_size)
{
    memory_region_count = 0;
    uint8_t *ptr = (uint8_t *)uefi_map;

    for (UINTN i = 0; i < map_size / desc_size && memory_region_count < MAX_MEMORY_REGIONS; i++) {
        uint64_t phys = *(uint64_t *)(ptr + 0);   /* PhysicalStart */
        uint64_t pages = *(uint64_t *)(ptr + 8);  /* NumberOfPages */
        uint32_t type = *(uint32_t *)(ptr + 16);   /* Type */
        uint64_t length = pages * 4096;

        uint32_t aurelian_type;
        switch (type) {
            case EFI_CONVENTIONAL_MEMORY:
                aurelian_type = AURELION_MEM_USABLE;
                break;
            case EFI_ACPI_RECLAIM_MEMORY:
                aurelian_type = AURELION_MEM_ACPI_RECLAIMABLE;
                break;
            case EFI_ACPI_MEMORY_NVS:
                aurelian_type = AURELION_MEM_ACPI_NVS;
                break;
            default:
                aurelian_type = AURELION_MEM_RESERVED;
                break;
        }

        if (length > 0) {
            memory_regions[memory_region_count].base = phys;
            memory_regions[memory_region_count].length = length;
            memory_regions[memory_region_count].type = aurelian_type;
            memory_regions[memory_region_count].flags = 0;
            memory_region_count++;
        }

        ptr += desc_size;
    }
}

/* Entry point */
EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    gST = SystemTable;

    gST->ConOut->ClearScreen(gST->ConOut);
    print_str("Aurelian UEFI Bootloader v0.0.1\n");

    /* ---- Locate RSDP ---- */
    uint64_t rsdp_physical = 0;
    for (UINTN i = 0; i < gST->NumberOfTableEntries; i++) {
        EFI_GUID *guid = (EFI_GUID *)((uint8_t *)gST->ConfigurationTable +
                                       i * sizeof(void *) * 2);
        if (memcmp(guid, &gEfiAcpi20TableGuid, sizeof(EFI_GUID)) == 0) {
            rsdp_physical = (uint64_t)(uintptr_t)((uint8_t *)gST->ConfigurationTable +
                            i * sizeof(void *) * 2 + sizeof(EFI_GUID));
            break;
        }
        if (memcmp(guid, &gEfiAcpi10TableGuid, sizeof(EFI_GUID)) == 0 &&
            rsdp_physical == 0) {
            rsdp_physical = (uint64_t)(uintptr_t)((uint8_t *)gST->ConfigurationTable +
                            i * sizeof(void *) * 2 + sizeof(EFI_GUID));
        }
    }

    print_str("RSDP at ");
    print_hex(rsdp_physical);
    print_str("\n");

    /* ---- Locate GOP ---- */
    struct aurelion_framebuffer fb = {0};
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    gST->BootServices->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (void **)&gop);
    if (gop) {
        EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *mode = gop->Mode;
        fb.address = mode->FrameBufferBase;
        fb.width = mode->Info->HorizontalResolution;
        fb.height = mode->Info->VerticalResolution;
        fb.pitch = mode->Info->PixelsPerScanLine * 4;
        fb.bpp = 32;
        set_framebuffer_format(&fb, mode->Info);

        print_str("GOP framebuffer: ");
        print_u64(fb.width);
        print_str("x");
        print_u64(fb.height);
        print_str("\n");
    } else {
        print_str("WARNING: No GOP available, framebuffer disabled.\n");
    }

    /* ---- Open the volume and read aurelion.elf ---- */
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = NULL;
    EFI_STATUS status = gST->BootServices->HandleProtocol(ImageHandle,
        &gEfiSimpleFileSystemProtocolGuid, (void **)&fs);
    if (EFI_ERROR(status)) {
        print_str("ERROR: Cannot get SimpleFileSystem protocol.\n");
        return status;
    }

    EFI_FILE_PROTOCOL *root = NULL;
    status = fs->OpenVolume(fs, &root);
    if (EFI_ERROR(status)) {
        print_str("ERROR: Cannot open volume.\n");
        return status;
    }

    void *elf_data = NULL;
    UINTN elf_size = 0;

    /* Try both paths */
    CHAR16 kernel_path[] = { '\\','E','F','I','\\','B','O','O','T','\\','a','u','r','e','l','i','o','n','.','e','l','f', 0 };
    status = read_file(root, kernel_path, &elf_data, &elf_size);
    if (EFI_ERROR(status)) {
        CHAR16 alt_path[] = { '\\','a','u','r','e','l','i','o','n','.','e','l','f', 0 };
        status = read_file(root, alt_path, &elf_data, &elf_size);
    }
    if (EFI_ERROR(status)) {
        print_str("ERROR: Cannot find aurelion.elf on ESP.\n");
        return EFI_NOT_FOUND;
    }

    print_str("Kernel ELF loaded: ");
    print_u64(elf_size);
    print_str(" bytes\n");

    /* ---- Validate ELF header ---- */
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)elf_data;
    /* Adjacent-literal split avoids "\x7fE" being parsed as one hex escape
     * (E is a valid hex digit, so "\x7fELF" alone corrupts the magic). */
    if (memcmp(ehdr->e_ident, "\x7f" "ELF", 4) != 0) {
        print_str("ERROR: Not a valid ELF file.\n");
        return EFI_LOAD_ERROR;
    }
    if (ehdr->e_ident[4] != 2) { /* ELFCLASS64 */
        print_str("ERROR: Not a 64-bit ELF.\n");
        return EFI_LOAD_ERROR;
    }
    if (ehdr->e_machine != 0x3E) { /* EM_X86_64 */
        print_str("ERROR: Not x86-64 ELF.\n");
        return EFI_LOAD_ERROR;
    }

    print_str("ELF entry point: ");
    print_hex(ehdr->e_entry);
    print_str("\n");

    /* ---- Load ELF segments ---- */
    uint64_t kernel_phys_base = 0xFFFFFFFFFFFFFFFF;
    uint64_t kernel_virt_base = 0xFFFFFFFFFFFFFFFF;

    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        Elf64_Phdr *phdr = (Elf64_Phdr *)((uint8_t *)elf_data + ehdr->e_phoff +
                                            i * ehdr->e_phentsize);

        if (phdr->p_type != PT_LOAD) continue;
        if (phdr->p_memsz == 0) continue;

        uint64_t paddr = phdr->p_paddr;
        uint64_t vaddr = phdr->p_vaddr;
        uint64_t filesz = phdr->p_filesz;
        uint64_t memsz = phdr->p_memsz;

        if (paddr < kernel_phys_base) kernel_phys_base = paddr;
        if (vaddr < kernel_virt_base) kernel_virt_base = vaddr;

        /* Allocate pages for this segment */
        uint64_t num_pages = (memsz + 4095) / 4096;
        EFI_PHYSICAL_ADDRESS seg_addr = paddr;

        status = gST->BootServices->AllocatePages(EFI_ALLOCATE_ADDRESS,
                                                   EFI_LOADER_DATA,
                                                   (UINTN)num_pages, &seg_addr);
        if (EFI_ERROR(status)) {
            print_str("ERROR: Cannot allocate pages for segment at ");
            print_hex(paddr);
            print_str("\n");
            return EFI_OUT_OF_RESOURCES;
        }

        /* Zero the entire memory region */
        memset((void *)(uintptr_t)seg_addr, 0, num_pages * 4096);

        /* Copy file data */
        if (filesz > 0) {
            memcpy((void *)(uintptr_t)seg_addr,
                   (uint8_t *)elf_data + phdr->p_offset,
                   filesz);
        }

        print_str("  Loaded segment: phys=");
        print_hex(seg_addr);
        print_str(" size=");
        print_u64(memsz);
        print_str("\n");
    }

    /* ---- Get memory map and exit boot services ---- */
    UINTN map_size = 0, map_key = 0, desc_size = 0;
    UINT32 desc_version = 0;

    /* First call to get buffer size */
    gST->BootServices->GetMemoryMap(&map_size, NULL, &map_key, &desc_size, &desc_version);
    map_size += 4096; /* Extra space for new allocations */

    void *memory_map = NULL;
    status = gST->BootServices->AllocatePool(EFI_LOADER_DATA, map_size, &memory_map);
    if (EFI_ERROR(status)) {
        print_str("ERROR: Cannot allocate memory map buffer.\n");
        return status;
    }

    status = gST->BootServices->GetMemoryMap(&map_size, memory_map,
                                             &map_key, &desc_size, &desc_version);
    if (EFI_ERROR(status)) {
        print_str("ERROR: Cannot get memory map.\n");
        return status;
    }

    /* Convert memory map before exiting boot services */
    convert_memory_map(memory_map, map_size, desc_size);
    print_str("Memory map: ");
    print_u64(memory_region_count);
    print_str(" regions\n");

    /* Exit boot services */
    status = gST->BootServices->ExitBootServices(ImageHandle, map_key);
    if (EFI_ERROR(status)) {
        /*
         * Memory map may have changed; retry once.
         * We can't print after this in most implementations,
         * so just proceed.
         */
        gST->BootServices->GetMemoryMap(&map_size, memory_map,
                                         &map_key, &desc_size, &desc_version);
        convert_memory_map(memory_map, map_size, desc_size);
        status = gST->BootServices->ExitBootServices(ImageHandle, map_key);
    }

    /* ---- Build boot info and jump to kernel ---- */
    static struct aurelion_boot_info boot_info;
    memset(&boot_info, 0, sizeof(boot_info));
    boot_info.magic = AURELION_BOOT_MAGIC;
    boot_info.abi_version = AURELION_BOOT_ABI_VERSION;
    boot_info.struct_size = sizeof(boot_info);
    boot_info.kernel_physical_base = kernel_phys_base;
    boot_info.kernel_virtual_base = kernel_virt_base;
    boot_info.rsdp_physical = rsdp_physical;
    boot_info.memory_region_count = memory_region_count;
    boot_info.memory_regions_physical = (uint64_t)(uintptr_t)&memory_regions;
    boot_info.framebuffer = fb;

    /* Jump to kernel entry point with boot_info in rdi */
    void (*kernel_entry)(struct aurelion_boot_info *) =
        (void (*)(struct aurelion_boot_info *))(uintptr_t)ehdr->e_entry;

    kernel_entry(&boot_info);

    /* Should never reach here */
    return EFI_LOAD_ERROR;
}
