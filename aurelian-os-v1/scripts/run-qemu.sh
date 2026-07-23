#!/usr/bin/env bash
# ============================================================================
# Aurelian OS — boot the ISO in QEMU
# ============================================================================
set -euo pipefail
cd "$(dirname "$0")/.."

if [[ ! -f aurelian-os.iso ]]; then
  echo ":: ISO not found, building first..."
  ./scripts/build-iso.sh
fi

echo ":: Launching Aurelian OS in QEMU (Ctrl-A X to quit)..."
exec qemu-system-x86_64 -cdrom aurelian-os.iso -m 256M -serial stdio
