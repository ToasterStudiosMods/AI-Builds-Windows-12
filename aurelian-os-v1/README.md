# Aurelian OS

An original, Windows 12-inspired desktop operating system concept for x86-64 PCs
and virtual machines. This repository contains the **source that builds a
bootable hybrid ISO** — an original kernel (`Aurelion`), a Multiboot2 boot
flow, and the start of the Luma Shell / Prism UI stack described in the project
blueprint.

> **Status — v1 baseline (bootable, x86-64 long mode).** The Aurelion kernel
> boots via GRUB (Multiboot2), transitions from 32-bit protected mode to
> **64-bit long mode** using 4-level paging, initializes a 64-bit GDT, drives
> the VGA text console, parses Multiboot2 boot info, and prints a boot banner.
> The scheduler, filesystems, and Luma Shell desktop are tracked as v2+
> milestones (see the [Roadmap](#roadmap)).

---

## Repository layout

```
aurelian-os/
├── Makefile                      # kernel / iso / run / clean targets
├── README.md
├── grub/
│   └── grub.cfg                  # GRUB boot menu (multiboot2 entry)
├── scripts/
│   ├── build-iso.sh              # one-shot: kernel + ISO
│   └── run-qemu.sh               # boot the ISO in QEMU
└── kernel/
    ├── kernel.c                  # C entry point: kmain()
    ├── vga.c                     # VGA text-mode driver (80×25)
    ├── string.c                  # freestanding mem/str helpers
    ├── include/
    │   ├── kernel.h
    │   ├── vga.h
    │   ├── string.h
    │   └── gdt.h
    └── arch/x86_64/
        ├── boot.S                # Multiboot2 header + 32→64-bit long-mode transition
        ├── gdt.c                 # 64-bit runtime GDT setup
        ├── gdt_flush.S           # lgdt + segment reload (asm)
        └── linker.ld             # link script: ELF64, 1 MiB load, 2 PT_LOAD segs
```

---

## Build

### 1. Install the toolchain (Debian/Ubuntu)

```bash
sudo apt install build-essential xorriso grub-pc-bin grub-efi-amd64-bin mtools qemu-system-x86
```

| Tool | Used by | Purpose |
|---|---|---|
| `gcc`, `ld`, `make` | `make kernel` | Compile + link the freestanding kernel |
| `xorriso`, `grub-mkrescue`, `mtools` | `make iso` | Build the hybrid (BIOS+UEFI) ISO |
| `grub-pc-bin`, `grub-efi-amd64-bin` | `make iso` | GRUB modules for BIOS + UEFI targets |
| `qemu-system-x86` | `make run` | Boot-test the ISO in a VM |

> The **kernel** builds with a plain freestanding `gcc` + `ld` — no special
> toolchain. Only the **ISO** and **run** steps need the extra packages.

### 2. Build the kernel

```bash
make kernel
```

Produces `kernel/aurelion.elf` — a valid Multiboot2 ELF (Intel 80386, entry
`_start`, loaded at 1 MiB). You can verify the Multiboot2 header with:

```bash
grub-file --is-x86-multiboot2 kernel/aurelion.elf && echo "valid multiboot2"
```

### 3. Build the bootable ISO

```bash
make iso
```

Produces `aurelian-os.iso` — a hybrid ISO that boots on BIOS **and** UEFI
firmware, in optical media, hypervisors (QEMU/VirtualBox/VMware), and when
flashed to a USB drive.

### 4. Run it

```bash
make run
# or directly:
qemu-system-x86_64 -cdrom aurelian-os.iso -m 256M
```

Flash to USB (bootable on real hardware):

```bash
sudo dd if=aurelian-os.iso of=/dev/sdX bs=4M conv=fsync status=progress
```

---

## What the v1 kernel does

1. **GRUB** loads `aurelion.elf` at 1 MiB and jumps to `_start` (32-bit
   protected mode, paging off — this is how every Multiboot2 loader enters).
2. `_start` (in `boot.S`, 32-bit):
   - Sets up a temporary 32-bit GDT and stack.
   - Builds a **4-level page table** (PML4 → PDPT → PD) identity-mapping the
     first 1 GiB with 2 MiB huge pages.
   - Enables **PAE** (CR4.PAE), loads CR3, sets **EFER.LME** (long-mode
     enable), then enables paging → CPU enters compatibility mode.
   - Loads the 64-bit GDT and **far-jumps** to a 64-bit code segment →
     **long mode**.
3. `long_mode_start` (64-bit) sets up a 64-bit stack and calls `kmain`.
4. `kmain` (in `kernel.c`, compiled `-m64`):
   - Initializes the **VGA text console** (`vga.c`) and prints the Aurelian
     banner.
   - Loads a clean runtime **64-bit GDT** (`gdt.c` + `gdt_flush.S`).
   - Walks the **Multiboot2 info tags** and prints the bootloader name and
     detected memory (lower/upper KiB).
   - Halts.
5. CPU sits in an `hlt` loop (interrupts disabled) — the v1 idle "scheduler".

### Verified in this build

- `make kernel` compiles cleanly with `gcc 14` (freestanding, `-m64`,
  `-mno-red-zone`, `-mcmodel=large`).
- `aurelion.elf` is a valid **ELF64 x86-64** executable, links with no
  warnings (two `PT_LOAD` segments: R+X text, R+W data — no RWX segment).
- Multiboot2 header is valid: magic `0xe85250d6`, arch `0` (i386 entry —
  correct, GRUB always enters 32-bit), length `24`, checksum `0x17adaf12`.
- Symbols present: `_start` (32-bit entry), `long_mode_start` (64-bit entry
  after the far jump), `kmain` (C entry in long mode), `gdt_flush`,
  `pml4`/`pdpt` (bootstrap page tables).

---

## Roadmap

The blueprint describes a full desktop OS. v1 is the bootable kernel
baseline; the rest is staged:

| Milestone | Status | Notes |
|---|---|---|
| Multiboot2 boot + long mode (x86-64) + 4-level paging + VGA + GDT | ✅ **v1** | this repo |
| Serial / debug port | next | `outb(0x3F8, ...)` — printf to QEMU `-serial` |
| Physical memory bitmap allocator | next | parse mbi2 mmap tag |
| Higher-half kernel mapping | next | move kernel to 0xFFFFFFFF80000000 |
| Preemptive scheduler + IRQs/PIC-IOAPIC | v2 | TSS, timer tick |
| `aurinit` service manager | v2 | dependency-ordered boot |
| ext4 read, `asfs` snapshot FS | v2 | VFS layer |
| `lumad` compositor + `luma-shell` | v2+ | framebuffer/simplefb first |
| Prism UI toolkit + native apps | v2+ | files, terminal, editor, … |
| Installer (`aurelian-installer`) | v2+ | partitioner, LUKS2 |
| `aurelian-updater` atomic updates | v3 | A/B images, rollback |

---

## Design notes

- **Kernel language:** C + GNU assembler (no `nasm` dependency). The kernel
  runs in **64-bit long mode** with 4-level (PML4) paging, matching the
  blueprint's "original x86-64 kernel" / "four-level page tables for v1".
- **No proprietary code or assets.** The Aurelian name, Luma Shell, Prism UI,
  and the `Aurelion` kernel are original. No Microsoft code, branding, icons,
  sounds, or implementation details are used.
- **Freestanding.** The kernel links with `-nostdlib` and provides its own
  `memset`/`memcpy`/`strlen`/`strcmp`/`strcpy` and integer-to-string helpers.
  No libgcc 64-bit helpers are required in v1.

---

## License

Original clean-room project code. See `LICENSE` (permissive, to be added).
