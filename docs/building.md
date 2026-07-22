# Building the early Aurelion kernel

This repository now includes the first implementation slice for the from-scratch Aurelian OS kernel, `Aurelion`.

## What exists

- A stable boot information ABI in `kernel/include/aurelian/boot.h`.
- A freestanding x86-64 kernel entry point in `kernel/src/x86_64/start.S`.
- A minimal kernel main routine in `kernel/src/kernel.c` that validates the boot ABI and halts safely.
- A VGA text console in `kernel/src/console.c` for the earliest bring-up logs.
- A linker script that places the kernel at the 1 MiB physical load address.
- A top-level `Makefile` that emits `build/aurelion.elf` and `build/aurelion.bin`.

## Build commands

```sh
make clean
make check
```

`make check` builds the freestanding kernel and compiles a host-side ABI check translation unit. The next implementation step is a small UEFI loader that fills `struct aurelion_boot_info` and jumps to `_start`.
