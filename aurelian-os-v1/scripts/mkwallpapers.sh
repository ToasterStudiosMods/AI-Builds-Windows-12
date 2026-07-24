#!/usr/bin/env bash
# ============================================================================
# Aurelian OS — bundle wallpapers as Multiboot2 modules + generate grub.cfg
#
# Each assets/wallpapers/*.{jpg,jpeg,png} becomes /boot/wpN.aow — a raw
# 1024x768 BGRX image with an "AOWP" header — loaded as a module named "wpN".
# The kernel picks them up (wp0 is the default) and the Settings app switches
# between them at runtime.
#
# Usage: scripts/mkwallpapers.sh <isodir>
# ============================================================================
set -e
ISODIR="$1"
mkdir -p "$ISODIR/boot/grub"
LINES="$ISODIR/boot/.wplines"; : > "$LINES"
# AOWP header = magic + width=1024 + height=768 (little-endian). printf writes
# the NUL bytes straight to the file (a shell variable cannot hold NULs).
write_hdr() { printf 'AOWP\000\004\000\000\000\003\000\000' > "$1"; }

shopt -s nullglob
mapfile -t WPS < <(ls assets/wallpapers/*.jpg assets/wallpapers/*.jpeg assets/wallpapers/*.png 2>/dev/null | sort)

emit() {  # emit <index> <source-image>
    local idx="$1" src="$2"
    convert "$src" -resize 1024x768^ -gravity center -extent 1024x768 -depth 8 \
            "bgra:$ISODIR/boot/.pix"
    write_hdr "$ISODIR/boot/wp$idx.aow"
    cat "$ISODIR/boot/.pix" >> "$ISODIR/boot/wp$idx.aow"
    echo "  module2 /boot/wp$idx.aow wp$idx" >> "$LINES"
}

if [ ${#WPS[@]} -eq 0 ]; then
    echo "  no wallpapers found; generating a default gradient"
    convert -size 1024x768 -define gradient:angle=135 gradient:'#3a2a7a'-'#0d0a1c' \
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

# generate grub.cfg with the wallpaper module lines
{
    echo "set timeout=3"
    echo "set default=0"
    echo "insmod all_video"
    echo "set gfxpayload=keep"
    echo 'menuentry "Aurelian OS 1.0.0-dev (Luma)" {'
    echo "  multiboot2 /boot/aurelion.elf"
    cat "$LINES"
    echo "  boot"
    echo "}"
} > "$ISODIR/boot/grub/grub.cfg"
rm -f "$LINES"
echo "  wrote grub.cfg"
