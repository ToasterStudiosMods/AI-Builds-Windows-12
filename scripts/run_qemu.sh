#!/bin/bash
# run_qemu.sh - Boot Aurelian OS in QEMU
#
# Prerequisites:
#   - QEMU installed (apt install qemu-system-x86)
#   - Kernel and stage-1 built (make all)
#
# Usage:
#   ./scripts/run_qemu.sh            # Serial output only (headless)
#   ./scripts/run_qemu.sh --graphic  # With VGA display
#   ./scripts/run_qemu.sh --debug    # With QEMU debug output

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build"

KERNEL_ELF="$BUILD_DIR/aurelion.elf"
STAGE1_BIN="$BUILD_DIR/stage1.bin"

if [ ! -f "$KERNEL_ELF" ]; then
    echo "ERROR: Kernel not built. Run 'make all' first."
    exit 1
fi

if [ ! -f "$STAGE1_BIN" ]; then
    echo "ERROR: Stage-1 bootloader not built. Run 'make all' first."
    exit 1
fi

MEMORY="${AURELIAN_QEMU_MEM:-1024}"

QEMU_ARGS=(
    -machine q35
    -m "$MEMORY"
    -kernel "$STAGE1_BIN"
    -initrd "$KERNEL_ELF"
    -serial stdio
    -no-reboot
)

if [[ "$1" == "--graphic" ]]; then
    echo "Starting Aurelian OS in QEMU with graphics (Ctrl+B then X to exit)..."
    shift
elif [[ "$1" == "--debug" ]]; then
    echo "Starting Aurelian OS in QEMU with debug output..."
    QEMU_ARGS+=(-d int,cpu_reset)
    shift
else
    echo "Starting Aurelian OS in QEMU (headless, serial output)..."
    echo "Press Ctrl+A then X to exit."
    QEMU_ARGS+=(-display none)
    shift
fi

QEMU_ARGS+=("$@")
exec qemu-system-x86_64 "${QEMU_ARGS[@]}"
