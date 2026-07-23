# Building the Aurelian OS kernel and bootloader

This repository contains the Aurelion kernel and a multiboot stage-1 bootloader for the Aurelian OS project.

## What exists

### Kernel (`aurelion`)
- Stable boot information ABI in `kernel/include/aurelian/boot.h`.
- x86-64 entry point in `kernel/src/x86_64/start.S`.
- Kernel main routine in `kernel/src/kernel.c` that initializes all subsystems.
- VGA text console in `kernel/src/console.c`.
- GOP framebuffer renderer in `kernel/src/framebuffer.c`. On a UEFI boot with
  a 24- or 32-bit RGB framebuffer, it displays the original Prism boot canvas
  after the virtual memory manager maps the display surface.
- Serial port (COM1) output in `kernel/src/x86_64/serial.c`.
- GDT (Global Descriptor Table) in `kernel/src/x86_64/gdt.c`.
- IDT (Interrupt Descriptor Table) with 32 exception handlers in `kernel/src/x86_64/idt.c`.
- Physical memory manager (bitmap-based) in `kernel/src/x86_64/pmm.c`.
- Virtual memory manager (4-level paging, higher-half) in `kernel/src/x86_64/vmm.c`.
- Kernel heap allocator (free-list) in `kernel/src/x86_64/heap.c`.
- Linker script placing the kernel at the 1 MiB physical load address.

### Bootloader (`stage1`)
- Multiboot 1 compliant bootloader in `boot/stage1/`.
- Loads kernel ELF from a multiboot module.
- Transitions from 32-bit protected mode to 64-bit long mode.
- Builds `struct aurelion_boot_info` from multiboot memory map.
- Sets up identity and higher-half page tables.

### UEFI bootloader (optional)
- UEFI application (`boot/uefi/`) for direct UEFI boot.
- Requires `x86_64-w64-mingw32-gcc` cross-compiler to build.
- Reads kernel ELF from EFI System Partition.
- Collects UEFI memory map, ACPI RSDP, GOP framebuffer info.
- Exits UEFI boot services and jumps to kernel.

## Build commands

```sh
make clean && make check   # Build kernel + ABI check (baseline)
make all                  # Build kernel + stage-1 bootloader
make kernel               # Build kernel only
make stage1               # Build stage-1 bootloader only
make uefi                 # Build UEFI bootloader (if cross-compiler available)
```

## Running in QEMU

### Prerequisites
```sh
apt install qemu-system-x86   # Debian/Ubuntu
```

### Headless (serial output)
```sh
make run-qemu
# or
./scripts/run_qemu.sh
```

### With graphical display
```sh
make run-qemu-graphical
# or
./scripts/run_qemu.sh --graphic
```

### With debug output
```sh
make run-qemu-debug
# or
./scripts/run_qemu.sh --debug
```

### Expected serial output
```
Aurelian stage-1 bootloader v0.0.1
Multiboot OK
Memory regions: N
Kernel ELF OK
Segments loaded
Entering long mode...
Aurelion kernel 0.0.1
Boot ABI validated.
  Memory regions: N
  Kernel phys base: 1048576
  RSDP physical: 0
GDT: initializing...
GDT: done.
IDT: initializing...
IDT: done (interrupts enabled).
PMM: initializing...
PMM: initialized, N free pages (M total).
VMM: initializing...
VMM: 4-level paging initialized (identity + higher-half).
HEAP: initializing...
HEAP: initialized at 4194304, size 4194304 bytes.
HEAP: test alloc of 64 bytes succeeded.
HEAP: test free succeeded.

=== Aurelion kernel bring-up complete ===
All subsystems initialized. Halting.
```

## Architecture

```
QEMU / BIOS
    |
    v
[stage1.bin - Multiboot bootloader (32-bit)]
    |-- Reads multiboot info
    |-- Loads kernel ELF segments
    |-- Builds boot_info struct
    |-- Sets up paging (identity + higher-half)
    |-- Transitions to 64-bit long mode
    |
    v
[aurelion.elf - Kernel (64-bit)]
    |-- serial_init() - COM1 serial port
    |-- Boot ABI validation
    |-- gdt_init() - Global Descriptor Table
    |-- idt_init() - Interrupt Descriptor Table (interrupts enabled)
    |-- pmm_init() - Physical memory manager (from memory map)
    |-- vmm_init() - Virtual memory manager (4-level paging + early 16 MiB map)
    |-- heap_init() - Kernel heap allocator
    |-- framebuffer map + Prism boot canvas (when supplied by UEFI GOP)
    |-- Boot complete, halt
```

## File structure

```
kernel/
  include/aurelian/
    boot.h       - Boot ABI definitions
    kernel.h     - Kernel entry points and console
    serial.h     - Serial port API
    gdt.h        - GDT definitions
    idt.h        - IDT definitions
    pmm.h        - Physical memory manager API
    vmm.h        - Virtual memory manager API
    heap.h       - Heap allocator API
  src/
    kernel.c     - Kernel main (orchestrates all subsystems)
    console.c    - VGA text console
    x86_64/
      start.S    - x86-64 entry stub
      serial.c   - COM1 serial output
      gdt.c      - GDT setup and load
      idt.c      - IDT with 32 exception handlers
      pmm.c      - Bitmap physical page allocator
      vmm.c      - 4-level paging, higher-half mapping
      heap.c     - Free-list heap allocator
  linker.ld     - Kernel linker script (load at 1 MiB)

boot/
  stage1/
    multiboot.c  - 32-bit C bootloader (ELF loading, memory map)
    start32.S    - 32-bit bootstrap (GDT) + 32->64 transition
    linker.ld    - Stage-1 linker script
  uefi/
    include/efi/ - Standalone EFI type definitions
    src/main.c   - UEFI application that loads and jumps to kernel
    linker.ld    - UEFI app linker script

scripts/
  abi_check.c   - Host-side ABI verification
  run_qemu.sh   - QEMU launch script

Makefile         - Build system (kernel, stage1, uefi, qemu targets)
```
