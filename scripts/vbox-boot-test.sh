#!/usr/bin/env bash
# ============================================================================
# Aurelian OS — VirtualBox headless boot smoke test
# ============================================================================
# CI builds the ISOs but never boots them. This script boots an ISO in a
# throwaway VirtualBox VM, captures the guest's serial output and the VGA text
# framebuffer, and fails if the guest triple-faults (VirtualBox "guru
# meditation"). It is the automated equivalent of "does the kernel actually
# come up?".
#
# Usage:
#   scripts/vbox-boot-test.sh <iso-path> [vm-name] [--marker "text"] [--seconds N]
#
# Examples:
#   scripts/vbox-boot-test.sh aurelian-os-v1/aurelian-os.iso
#   scripts/vbox-boot-test.sh build/aurelian.iso Aurelion_TL --marker "Boot ABI validated"
#
# Exit status:
#   0  guest booted and stayed alive (and matched --marker, if given)
#   1  guru meditation / triple fault, or marker not found, or setup error
#
# Requires: VBoxManage (Oracle VirtualBox). Works on Linux/macOS and on Windows
# via Git Bash / MSYS2 (auto-detects the default install path).
# ============================================================================
set -u

# ---- locate VBoxManage ----
VBM="${VBOXMANAGE:-}"
if [ -z "$VBM" ]; then
    if command -v VBoxManage >/dev/null 2>&1; then
        VBM="$(command -v VBoxManage)"
    elif [ -x "/c/Program Files/Oracle/VirtualBox/VBoxManage.exe" ]; then
        VBM="/c/Program Files/Oracle/VirtualBox/VBoxManage.exe"
    elif [ -x "/mnt/c/Program Files/Oracle/VirtualBox/VBoxManage.exe" ]; then
        VBM="/mnt/c/Program Files/Oracle/VirtualBox/VBoxManage.exe"
    else
        echo "ERROR: VBoxManage not found. Set \$VBOXMANAGE or install VirtualBox." >&2
        exit 1
    fi
fi

# ---- args ----
ISO="${1:-}"
VM="${2:-OS_Boot_Test}"
MARKER=""
SECONDS_TOTAL=18
shift $(( $# < 2 ? $# : 2 )) 2>/dev/null || true
while [ $# -gt 0 ]; do
    case "$1" in
        --marker)  MARKER="${2:-}"; shift 2 ;;
        --seconds) SECONDS_TOTAL="${2:-18}"; shift 2 ;;
        *) shift ;;
    esac
done

if [ -z "$ISO" ] || [ ! -f "$ISO" ]; then
    echo "ERROR: ISO not found: '$ISO'" >&2
    echo "Usage: $0 <iso-path> [vm-name] [--marker \"text\"] [--seconds N]" >&2
    exit 1
fi

# VBoxManage on Windows wants a native path for --medium; convert if cygpath exists.
ISO_NATIVE="$ISO"
if command -v cygpath >/dev/null 2>&1; then
    ISO_NATIVE="$(cygpath -w "$ISO")"
fi

OUT="$(mktemp -d 2>/dev/null || echo "./vbox-boot-$$")"
mkdir -p "$OUT"
SERIAL="$OUT/serial.log"
: > "$SERIAL"
log(){ echo "[vbox-boot-test] $*"; }

cleanup() {
    "$VBM" controlvm "$VM" poweroff >/dev/null 2>&1
    sleep 1
    "$VBM" unregistervm "$VM" --delete >/dev/null 2>&1
}
trap cleanup EXIT

# ---- fresh VM ----
"$VBM" controlvm "$VM" poweroff >/dev/null 2>&1
"$VBM" unregistervm "$VM" --delete >/dev/null 2>&1
sleep 1

log "creating VM '$VM'"
"$VBM" createvm --name "$VM" --ostype "Other_64" --register >/dev/null || exit 1
"$VBM" modifyvm "$VM" --memory 2048 --cpus 2 --firmware bios \
    --chipset ich9 --ioapic on --pae on --longmode on \
    --graphicscontroller vboxvga --vram 16 \
    --boot1 dvd --boot2 none --boot3 none --boot4 none \
    --nic1 none --uart1 0x3F8 4 --uartmode1 file "$SERIAL" >/dev/null || exit 1
"$VBM" storagectl "$VM" --name "SATA Controller" --add sata --bootable on >/dev/null || exit 1
"$VBM" storageattach "$VM" --storagectl "SATA Controller" --port 0 --device 0 \
    --type dvddrive --medium "$ISO_NATIVE" >/dev/null || exit 1

log "booting headless: $ISO"
"$VBM" startvm "$VM" --type headless >/dev/null || exit 1

# ---- observe ----
GURU=0
STEPS=$(( SECONDS_TOTAL / 3 )); [ "$STEPS" -lt 1 ] && STEPS=1
for i in $(seq 1 "$STEPS"); do
    sleep 3
    STATE="$("$VBM" showvminfo "$VM" --machinereadable 2>/dev/null | sed -n 's/^VMState=//p' | tr -d '"')"
    "$VBM" controlvm "$VM" screenshotpng "$OUT/screen_${i}.png" >/dev/null 2>&1
    log "t=$(( i * 3 ))s  state=$STATE"
    if [ "$STATE" = "gurumeditation" ]; then GURU=1; break; fi
done

# ---- evidence: serial + the VGA text dump VirtualBox writes into VBox.log ----
VMCFG="$("$VBM" showvminfo "$VM" --machinereadable 2>/dev/null | sed -n 's/^CfgFile=//p' | tr -d '"')"
VBOXLOG="$(dirname "$VMCFG")/Logs/VBox.log"

echo "----- serial.log -----"
if [ -s "$SERIAL" ]; then cat "$SERIAL"; else echo "(no serial output)"; fi
echo "----- VGA text (from VBox.log) -----"
[ -f "$VBOXLOG" ] && sed -n '/{vgatext}/,/^.*!!!!!!!!!!/p' "$VBOXLOG" | sed 's/^[0-9:.]* //'

# ---- verdict ----
RC=0
if [ "$GURU" -eq 1 ]; then
    log "RESULT: FAIL — guest triple-faulted (guru meditation)."
    RC=1
elif [ -n "$MARKER" ]; then
    if grep -qF "$MARKER" "$SERIAL" 2>/dev/null || \
       { [ -f "$VBOXLOG" ] && grep -qF "$MARKER" "$VBOXLOG"; }; then
        log "RESULT: PASS — marker found: '$MARKER'"
    else
        log "RESULT: FAIL — marker not found: '$MARKER'"
        RC=1
    fi
else
    log "RESULT: PASS — guest booted and stayed alive."
fi

log "artifacts in: $OUT"
exit $RC
