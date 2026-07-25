#!/usr/bin/env bash
# ============================================================================
# Aurelian OS — bundle wallpapers as Multiboot2 modules + generate grub.cfg
#
# Each assets/wallpapers/*.{jpg,jpeg,png} becomes /boot/wpN.aow — a raw
# 512x384 BGRX image with an "AOWP" header (upscaled by the kernel) — loaded as a module named "wpN".
# The kernel picks them up (wp0 is the default) and the Settings app switches
# between them at runtime.
#
# Usage: scripts/mkwallpapers.sh <isodir>
# ============================================================================
set -e
ISODIR="$1"
mkdir -p "$ISODIR/boot/grub"
LINES="$ISODIR/boot/.wplines"; : > "$LINES"
# Wallpapers are stored at 512x384 (a quarter of the 1024x768 screen) to keep
# the boot modules small/fast to load; the kernel upscales them. AOWP header =
# magic + width=512 + height=384 (little-endian). printf writes the NUL bytes
# straight to the file (a shell variable cannot hold NULs).
write_hdr() { printf 'AOWP\000\002\000\000\200\001\000\000' > "$1"; }

shopt -s nullglob
mapfile -t WPS < <(ls assets/wallpapers/*.jpg assets/wallpapers/*.jpeg assets/wallpapers/*.png 2>/dev/null | sort)

emit() {  # emit <index> <source-image>
    local idx="$1" src="$2"
    convert "$src" -resize 512x384^ -gravity center -extent 512x384 -depth 8 \
            "bgra:$ISODIR/boot/.pix"
    write_hdr "$ISODIR/boot/wp$idx.aow"
    cat "$ISODIR/boot/.pix" >> "$ISODIR/boot/wp$idx.aow"
    echo "  module2 /boot/wp$idx.aow wp$idx" >> "$LINES"
}

if [ ${#WPS[@]} -eq 0 ]; then
    echo "  no wallpapers found; generating a default gradient"
    convert -size 512x384 -define gradient:angle=135 gradient:'#3a2a7a'-'#0d0a1c' \
            -depth 8 "bgra:$ISODIR/boot/.gradpix"
    write_hdr "$ISODIR/boot/wp0.aow"
    cat "$ISODIR/boot/.gradpix" >> "$ISODIR/boot/wp0.aow"
    rm -f "$ISODIR/boot/.gradpix"
    echo "  module2 /boot/wp0.aow wp0" >> "$LINES"
else
    i=0
    for wp in "${WPS[@]}"; do
        echo "  wallpaper[$i] = $wp"
        emit "$i" "$wp"
        i=$((i + 1))
    done
    rm -f "$ISODIR/boot/.pix"
fi

# Generate grub.cfg with the wallpaper module lines. The echo lines are boot
# progress markers: if a load ever fails, GRUB stops with its error right after
# the last marker printed, which makes the failure obvious on a screenshot.
{
    # Mirror GRUB's own output to COM1 as well as the screen. VirtualBox does
    # not reliably expose the text-mode framebuffer to screenshots, so serial is
    # the only trustworthy record of what the loader did.
    echo "serial --unit=0 --speed=38400"
    echo "terminal_output console serial"
    echo "set timeout=3"
    echo "set default=0"
    echo "insmod all_video"
    echo "set gfxpayload=keep"
    echo 'menuentry "Aurelian OS 1.0.0-dev (Luma)" {'
    echo '  echo "Loading Aurelion kernel..."'
    echo "  multiboot2 /boot/aurelion.elf"
    echo '  echo "Loading wallpapers..."'
    cat "$LINES"
    echo '  echo "Starting Aurelian OS."'
    echo "  boot"
    echo "}"
} > "$ISODIR/boot/grub/grub.cfg"
rm -f "$LINES"
echo "  wrote grub.cfg"
