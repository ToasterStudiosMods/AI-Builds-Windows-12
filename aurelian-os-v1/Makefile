# ============================================================================
# Aurelian OS — Aurelion kernel build system
# ============================================================================
#
# Targets:
#   make kernel   — build kernel/aurelion.elf          (gcc + ld only)
#   make iso      — build aurelian-os.iso              (needs xorriso + grub-mkrescue)
#   make run      — boot the ISO in QEMU               (needs qemu-system-x86_64)
#   make clean    — remove build artifacts
#
# Toolchain requirements (Debian/Ubuntu):
#   sudo apt install build-essential xorriso grub-pc-bin grub-efi-amd64-bin \
#                    mtools qemu-system-x86
#
# The kernel compiles with a plain freestanding gcc + ld (no special
# toolchain). The ISO step additionally needs xorriso + grub-mkrescue.
# ============================================================================

# ---- toolchain ----
CC      := gcc
LD      := ld
# x86-64 long-mode kernel. -mcmodel=large keeps code/references reachable
# from the low 1 MiB load address. -mno-red-zone avoids stack-relative
# faults in interrupt contexts. -mno-sse/mmx/sse2 keeps us freestanding
# (no SSE save/restore machinery yet).
CFLAGS  := -m64 -ffreestanding -fno-pie -fno-stack-protector \
	   -fno-asynchronous-unwind-tables -fno-exceptions \
	   -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -mcmodel=large \
	   -Wall -Wextra -Wno-unused-parameter -O2 -g \
	   -Ikernel/include -Ikernel/arch/x86_64
LDFLAGS := -m elf_x86_64 -T kernel/arch/x86_64/linker.ld -nostdlib -z noexecstack

# ---- sources ----
C_SRCS  := kernel/kernel.c kernel/vga.c kernel/string.c kernel/arch/x86_64/gdt.c
ASM_SRCS := kernel/arch/x86_64/boot.S kernel/arch/x86_64/gdt_flush.S
OBJ     := $(C_SRCS:.c=.o) $(ASM_SRCS:.S=.o)

KERNEL  := kernel/aurelion.elf
ISO     := aurelian-os.iso
ISO_DIR := isodir

# ---- rules ----
.PHONY: all kernel iso run clean install-deps

all: kernel

kernel: $(KERNEL)

$(KERNEL): $(OBJ) kernel/arch/x86_64/linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJ)
	@echo ""
	@echo "  Built $(KERNEL)"
	@file $(KERNEL) 2>/dev/null || true
	@echo ""

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

# ---- ISO (needs xorriso + grub-mkrescue) ----
iso: $(KERNEL)
	@command -v grub-mkrescue >/dev/null 2>&1 || { \
	        echo "ERROR: grub-mkrescue not found. Install:"; \
	        echo "  sudo apt install xorriso grub-pc-bin grub-efi-amd64-bin mtools"; \
	        exit 1; }
	rm -rf $(ISO_DIR)
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL) $(ISO_DIR)/boot/aurelion.elf
	cp grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) $(ISO_DIR) 2>/dev/null
	rm -rf $(ISO_DIR)
	@echo ""
	@echo "  Built $(ISO) — flash to USB or boot in a VM."
	@echo ""

# ---- run in QEMU ----
run: iso
	@command -v qemu-system-x86_64 >/dev/null 2>&1 || { \
	        echo "ERROR: qemu-system-x86_64 not found. Install:"; \
	        echo "  sudo apt install qemu-system-x86"; \
	        exit 1; }
	qemu-system-x86_64 -cdrom $(ISO) -m 256M -serial stdio

# ---- convenience ----
install-deps:
	@echo "On Debian/Ubuntu:"
	@echo "  sudo apt install build-essential xorriso grub-pc-bin grub-efi-amd64-bin mtools qemu-system-x86"
	@echo "On Fedora:"
	@echo "  sudo dnf install gcc make xorriso grub2-efi-x64-modules grub2-pc-modules mtools qemu-system-x86"

clean:
	rm -f $(OBJ) $(KERNEL) $(ISO)
	rm -rf $(ISO_DIR)
