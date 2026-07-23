#!/usr/bin/env bash
# ============================================================================
# Aurelian OS — build the bootable ISO
# ============================================================================
set -euo pipefail

cd "$(dirname "$0")/.."

echo ":: Building Aurelion kernel..."
make kernel

echo ":: Building aurelian-os.iso..."
make iso

echo ""
echo "Done. Output:"
echo "  kernel/aurelion.elf   — Multiboot2 ELF kernel"
echo "  aurelian-os.iso       — bootable hybrid ISO (BIOS + UEFI)"
echo ""
echo "Boot it:"
echo "  qemu-system-x86_64 -cdrom aurelian-os.iso -m 256M"
echo "  or flash to USB:   dd if=aurelian-os.iso of=/dev/sdX bs=4M conv=fsync"
