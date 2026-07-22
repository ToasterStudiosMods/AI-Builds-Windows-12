# Handoff for GLM 5.2: Continue Aurelian OS / Aurelion Kernel

This repository currently contains a product blueprint plus the first tiny implementation slice of the from-scratch x86-64 kernel named **Aurelion**. Pick up from the current branch tip and continue by turning the freestanding kernel skeleton into a bootable ISO path.

## Current state

- `README.md` contains the full Aurelian OS product and technical blueprint.
- `Makefile` builds a freestanding kernel image:
  - `build/aurelion.elf`
  - `build/aurelion.bin`
- `kernel/include/aurelian/boot.h` defines the first boot ABI:
  - `AURELION_BOOT_MAGIC`
  - `AURELION_BOOT_ABI_VERSION`
  - memory map entries
  - framebuffer metadata
  - `struct aurelion_boot_info`
- `kernel/include/aurelian/kernel.h` declares the kernel entry points and early console API.
- `kernel/src/x86_64/start.S` is the x86-64 entry stub.
- `kernel/src/kernel.c` validates the boot ABI and writes early bring-up messages.
- `kernel/src/console.c` provides a VGA text console for early diagnostics.
- `kernel/linker.ld` places the kernel at the 1 MiB physical load address.
- `scripts/abi_check.c` gives a host-side header sanity check.
- `docs/building.md` documents the early build flow.

## Verified command

Run this first to confirm the same baseline:

```sh
make clean && make check
```

Expected result: the kernel compiles, links, emits `build/aurelion.elf` and `build/aurelion.bin`, and the ABI check compiles without errors.

## Immediate next objective

Build the missing **boot path** so the kernel can run under QEMU from a bootable ISO or UEFI disk image.

Recommended next PR title:

```text
Add UEFI loader and QEMU boot smoke path
```

## Recommended next implementation steps

### 1. Add a UEFI loader

Create a new module, for example:

```text
boot/uefi/
├── include/
├── src/
│   └── main.c
└── linker.ld or build flags as needed
```

The loader should:

1. Start as a UEFI application named `BOOTX64.EFI`.
2. Locate and load `aurelion.elf` from the EFI System Partition or ISO image.
3. Parse the ELF64 program headers.
4. Allocate pages for each loadable segment.
5. Copy kernel segments into memory and zero BSS ranges.
6. Gather the UEFI memory map.
7. Locate ACPI RSDP.
8. Query GOP framebuffer details.
9. Build `struct aurelion_boot_info` exactly as declared in `kernel/include/aurelian/boot.h`.
10. Exit boot services.
11. Jump to the kernel entry point, passing the boot info pointer in the first x86-64 SysV argument register (`rdi`).

Keep the boot ABI versioned. If `struct aurelion_boot_info` changes, update the static assertions and `scripts/abi_check.c` accordingly.

### 2. Add ISO/ESP layout generation

Extend `Makefile` with targets such as:

```make
esp
iso
run-qemu
```

A first simple layout can be:

```text
build/esp/
└── EFI/
    └── BOOT/
        ├── BOOTX64.EFI
        └── aurelion.elf
```

For ISO output, prefer a hybrid layout later; for the immediate smoke test, a FAT ESP disk image booted by OVMF is enough.

### 3. Add QEMU smoke boot

Add a script such as:

```text
scripts/run_qemu_uefi.sh
```

It should boot the generated ESP image with OVMF and either:

- show the VGA/graphics console, or
- write serial output to stdio once serial logging is implemented.

Useful QEMU shape:

```sh
qemu-system-x86_64 \
  -machine q35 \
  -m 1024 \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE.fd \
  -drive format=raw,file=build/aurelian-esp.img \
  -serial stdio
```

Adjust OVMF paths for the environment.

### 4. Improve early kernel diagnostics

Before or during the bootloader work, add serial output so CI can assert boot success without screenshot inspection.

Suggested files:

```text
kernel/include/aurelian/serial.h
kernel/src/x86_64/serial.c
```

Minimum behavior:

- Initialize COM1 at `0x3F8`.
- Provide `aurelion_serial_write(const char *text)`.
- Mirror kernel boot messages to serial and VGA.

### 5. Start architecture bring-up after boot works

After UEFI boot into `aurelion_kernel_main` works, implement in this order:

1. GDT setup.
2. IDT setup with default exception handlers.
3. Physical memory manager from the UEFI memory map.
4. Virtual memory manager and higher-half mapping plan.
5. Kernel heap allocator.
6. Interrupt controller detection: APIC/x2APIC path first, PIC fallback optional.
7. Timer: HPET or APIC timer, then scheduler tick.
8. Basic cooperative task abstraction, then preemptive scheduling.

Do not jump to UI work until the boot path, exception handling, and memory management are stable.

## Design constraints to preserve

- Keep Aurelian/Aurelion original. Do not copy Microsoft code, branding, names, icons, sounds, or proprietary assets.
- The kernel is a clean-room implementation, not a Linux fork.
- Keep public ABI structs explicit, fixed-width, and protected by static assertions.
- Prefer small, reviewable PRs.
- Keep `make check` passing after every change.
- Do not add try/catch around imports in any future language-specific code.

## Suggested acceptance criteria for the next PR

- `make check` still passes.
- `make run-qemu` or an equivalent documented command reaches `aurelion_kernel_main`.
- The kernel prints `Aurelion kernel 0.0.1` and `Boot ABI validated.` through a CI-observable channel, preferably serial.
- The UEFI loader validates kernel ELF headers and fails with a clear error if the image is malformed.
- The boot ABI remains documented and covered by compile-time size checks.

## Known gaps

- No UEFI loader exists yet.
- No real ISO image is generated yet.
- No serial logger exists yet.
- The current entry stub assumes the loader already placed the boot info pointer in `rdi`.
- There is no GDT/IDT, paging setup, allocator, interrupt handling, scheduler, or driver framework yet.
- VGA text output is only an early bring-up tool and should not become the final graphics path.
