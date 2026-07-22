CC ?= gcc
LD ?= ld
OBJCOPY ?= objcopy

BUILD_DIR := build
KERNEL_DIR := kernel
KERNEL_ELF := $(BUILD_DIR)/aurelion.elf
KERNEL_BIN := $(BUILD_DIR)/aurelion.bin

# Kernel flags (64-bit freestanding)
CFLAGS := -std=c11 -ffreestanding -fno-stack-protector -fno-pic -mno-red-zone \
          -Wall -Wextra -Werror -Ikernel/include
ASFLAGS := -ffreestanding -fno-pic -mno-red-zone
LDFLAGS := -nostdlib -z max-page-size=0x1000 -T $(KERNEL_DIR)/linker.ld

KERNEL_C_SOURCES := $(shell find $(KERNEL_DIR)/src -name '*.c' | sort)
KERNEL_S_SOURCES := $(shell find $(KERNEL_DIR)/src -name '*.S' | sort)
KERNEL_OBJECTS := $(patsubst $(KERNEL_DIR)/src/%,$(BUILD_DIR)/kernel/%,$(KERNEL_C_SOURCES:.c=.o)) \
                  $(patsubst $(KERNEL_DIR)/src/%,$(BUILD_DIR)/kernel/%,$(KERNEL_S_SOURCES:.S=.o))

# Stage-1 multiboot bootloader flags (32-bit)
STAGE1_DIR := boot/stage1
STAGE1_ELF := $(BUILD_DIR)/stage1.elf
STAGE1_BIN := $(BUILD_DIR)/stage1.bin

STAGE1_CFLAGS := -std=c11 -ffreestanding -fno-stack-protector -fno-pic \
                -m32 -Wall -Wextra -Werror -Ikernel/include
STAGE1_ASFLAGS := -ffreestanding -fno-pic -m32
STAGE1_LDFLAGS := -nostdlib -m elf_i386 -T $(STAGE1_DIR)/linker.ld

STAGE1_OBJECTS := $(BUILD_DIR)/stage1/multiboot.o \
                  $(BUILD_DIR)/stage1/start32.o

# UEFI bootloader (requires gnu-efi cross-compiler - optional)
BOOT_DIR := boot/uefi
BOOT_EFI := $(BUILD_DIR)/BOOTX64.EFI
BOOT_HAS_EFI := $(shell which x86_64-w64-mingw32-gcc 2>/dev/null)

.PHONY: all kernel clean check stage1 run-qemu run-qemu-debug iso esp

all: kernel stage1

# ===== Kernel =====

kernel: $(KERNEL_ELF) $(KERNEL_BIN)

$(BUILD_DIR)/kernel/%.o: $(KERNEL_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel/%.o: $(KERNEL_DIR)/src/%.S
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

$(KERNEL_ELF): $(KERNEL_OBJECTS) $(KERNEL_DIR)/linker.ld
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJECTS)

$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@

# ===== Stage-1 Multiboot Bootloader =====

stage1: $(STAGE1_ELF) $(STAGE1_BIN)

$(BUILD_DIR)/stage1/multiboot.o: $(STAGE1_DIR)/multiboot.c
	@mkdir -p $(dir $@)
	$(CC) $(STAGE1_CFLAGS) -c $< -o $@

$(BUILD_DIR)/stage1/start32.o: $(STAGE1_DIR)/start32.S
	@mkdir -p $(dir $@)
	$(CC) $(STAGE1_ASFLAGS) -c $< -o $@

$(STAGE1_ELF): $(STAGE1_OBJECTS) $(STAGE1_DIR)/linker.ld
	$(LD) $(STAGE1_LDFLAGS) -o $@ $(STAGE1_OBJECTS)

$(STAGE1_BIN): $(STAGE1_ELF)
	$(OBJCOPY) -O binary $< $@

# ===== UEFI Bootloader (optional, requires cross-compiler) =====

ifdef BOOT_HAS_EFI
uefi: $(BOOT_EFI)
else
uefi:
	@echo "UEFI bootloader requires x86_64-w64-mingw32-gcc (not found)."
	@echo "Install gnu-efi or mingw-w64 to build BOOTX64.EFI."
endif

$(BUILD_DIR)/uefi/%.o: $(BOOT_DIR)/src/%.c
	@mkdir -p $(dir $@)
	x86_64-w64-mingw32-gcc -std=c11 -ffreestanding -fno-stack-protector \
	    -Iboot/uefi/include -Ikernel/include -c $< -o $@

$(BOOT_EFI): $(BUILD_DIR)/uefi/main.o
	x86_64-w64-mingw32-ld -nostdlib -e efi_main -o $@ $< --subsystem 10

# ===== ESP Image =====

esp: $(BUILD_DIR)/aurelion-esp.img

$(BUILD_DIR)/aurelion-esp.img: kernel $(BOOT_EFI)
	@mkdir -p $(BUILD_DIR)/esp/EFI/BOOT
	cp $(KERNEL_ELF) $(BUILD_DIR)/esp/EFI/BOOT/aurelion.elf
ifdef BOOT_HAS_EFI
	cp $(BOOT_EFI) $(BUILD_DIR)/esp/EFI/BOOT/BOOTX64.EFI
endif
	@# Create a FAT32 disk image (use dd + mkfs if available, else skip)
	@if command -v mkfs.fat >/dev/null 2>&1; then \
		dd if=/dev/zero of=$@ bs=1M count=8 2>/dev/null; \
		mkfs.fat -F 32 $@ 2>/dev/null; \
		mmd -i $@ ::EFI ::EFI/BOOT 2>/dev/null; \
		mcopy -i $@ $(BUILD_DIR)/esp/EFI/BOOT/aurelion.elf ::EFI/BOOT/ 2>/dev/null; \
		echo "ESP image created: $@"; \
	else \
		echo "mkfs.fat not found - ESP image creation skipped."; \
	fi

# ===== ISO =====

iso: $(BUILD_DIR)/aurelian.iso
	@echo "ISO creation requires xorriso/grub-mkrescue. Install and re-run."

$(BUILD_DIR)/aurelian.iso: kernel stage1
	@if command -v grub-mkrescue >/dev/null 2>&1; then \
		mkdir -p $(BUILD_DIR)/iso/boot/grub; \
		cp $(KERNEL_ELF) $(BUILD_DIR)/iso/boot/aurelion.elf; \
		cp $(STAGE1_BIN) $(BUILD_DIR)/iso/boot/stage1.bin; \
		echo "menuentry 'Aurelian OS' {" > $(BUILD_DIR)/iso/boot/grub/grub.cfg; \
		echo "  multiboot /boot/stage1.bin" >> $(BUILD_DIR)/iso/boot/grub/grub.cfg; \
		echo "  module /boot/aurelion.elf" >> $(BUILD_DIR)/iso/boot/grub/grub.cfg; \
		echo "  boot" >> $(BUILD_DIR)/iso/boot/grub/grub.cfg; \
		echo "}" >> $(BUILD_DIR)/iso/boot/grub/grub.cfg; \
		grub-mkrescue -o $@ $(BUILD_DIR)/iso 2>/dev/null; \
		echo "ISO created: $@"; \
	else \
		echo "grub-mkrescue not found - ISO creation skipped."; \
	fi

# ===== QEMU Run Targets =====

run-qemu: kernel stage1
	qemu-system-x86_64 \
		-m 1024 \
		-kernel $(STAGE1_BIN) \
		-initrd $(KERNEL_ELF) \
		-serial stdio \
		-display none \
		-no-reboot

run-qemu-debug: kernel stage1
	qemu-system-x86_64 \
		-m 1024 \
		-kernel $(STAGE1_BIN) \
		-initrd $(KERNEL_ELF) \
		-serial stdio \
		-display none \
		-d int,cpu_reset \
		-no-reboot

run-qemu-graphical: kernel stage1
	qemu-system-x86_64 \
		-m 1024 \
		-kernel $(STAGE1_BIN) \
		-initrd $(KERNEL_ELF) \
		-serial stdio

# ===== Checks =====

check: $(KERNEL_ELF) $(KERNEL_BIN)
	$(CC) -std=c11 -Ikernel/include -c scripts/abi_check.c -o $(BUILD_DIR)/abi_check.o

clean:
	rm -rf $(BUILD_DIR)
