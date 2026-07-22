CC ?= gcc
LD ?= ld
OBJCOPY ?= objcopy

BUILD_DIR := build
KERNEL_DIR := kernel
KERNEL_ELF := $(BUILD_DIR)/aurelion.elf
KERNEL_BIN := $(BUILD_DIR)/aurelion.bin

CFLAGS := -std=c11 -ffreestanding -fno-stack-protector -fno-pic -mno-red-zone -Wall -Wextra -Werror -Ikernel/include
ASFLAGS := -ffreestanding -fno-pic -mno-red-zone
LDFLAGS := -nostdlib -z max-page-size=0x1000 -T $(KERNEL_DIR)/linker.ld

KERNEL_C_SOURCES := $(shell find $(KERNEL_DIR)/src -name '*.c' | sort)
KERNEL_S_SOURCES := $(shell find $(KERNEL_DIR)/src -name '*.S' | sort)
KERNEL_OBJECTS := $(patsubst $(KERNEL_DIR)/src/%,$(BUILD_DIR)/kernel/%,$(KERNEL_C_SOURCES:.c=.o)) \
                  $(patsubst $(KERNEL_DIR)/src/%,$(BUILD_DIR)/kernel/%,$(KERNEL_S_SOURCES:.S=.o))

.PHONY: all kernel clean check

all: kernel

kernel: $(KERNEL_ELF) $(KERNEL_BIN)

$(KERNEL_ELF): $(KERNEL_OBJECTS) $(KERNEL_DIR)/linker.ld
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJECTS)

$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/kernel/%.o: $(KERNEL_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel/%.o: $(KERNEL_DIR)/src/%.S
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

check: kernel
	$(CC) -std=c11 -Wall -Wextra -Werror -Ikernel/include -fsyntax-only scripts/abi_check.c

clean:
	rm -rf $(BUILD_DIR)
