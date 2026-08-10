ARCH=x86_64
CFLAGS = -g -Wall -fshort-wchar -fno-pie -ffreestanding -fpic -mno-red-zone -fno-stack-protector -nostdlib -I./base -D__$(ARCH)__
LDFLAGS = -g -nostdlib -n -T link.ld -no-pie
KNAME=osdev
override SRCFILES := $(shell find -L base -type f -not -path 'base/arch/*' 2>/dev/null | LC_ALL=C sort)
override SRCFILES += $(shell find -L base/arch/$(ARCH) -type f 2>/dev/null | LC_ALL=C sort)
override CFILES := $(filter %.c,$(SRCFILES))
override CFILES := $(filter %.c,$(SRCFILES))
override ASFILES := $(filter %.S,$(SRCFILES))
ifeq ($(ARCH),x86_64)
override NASMFILES := $(filter %.asm,$(SRCFILES))
endif
override OBJ := $(addprefix obj-$(ARCH)/,$(CFILES:.c=.c.o) $(ASFILES:.S=.S.o))
ifeq ($(ARCH),x86_64)
override OBJ += $(addprefix obj-$(ARCH)/,$(NASMFILES:.asm=.asm.o))
endif
all: preinit $(KNAME).x86_64.elf user drivers

.PHONY: FORCE

drivers: FORCE
	$(MAKE) -C drivers all cpsysroot ARCH=$(ARCH)

user: FORCE
	$(MAKE) -C user all cpsysroot ARCH=$(ARCH)

produceimage: user
	cp $(KNAME).$(ARCH).elf sysroot/boot/osdev.elf
	@exec ./produceimage.sh

# Fetch external sources (like `bootboot.h`)
preinit:
	@exec ./preinit.sh


$(KNAME).x86_64.elf: $(OBJ)
	@mkdir -p "$(dir $@)"
	@x86_64-elf-ld $(LDFLAGS) $(OBJ) -o $@
	@echo " $<"

obj-$(ARCH)/%.c.o: %.c
	@mkdir -p "$(dir $@)"
	@$(ARCH)-elf-gcc $(CFLAGS) -c $< -o $@
	@echo " $<"
obj-x86_64/%.asm.o: %.asm
	@mkdir -p "$(dir $@)"
	@nasm -f elf64 $< -o $@
	@echo " $<"

clean:
	@rm -rf obj-$(ARCH)
	@rm -rf $(KNAME).$(ARCH).elf
	@rm -rf sysroot/boot/*.elf
	@rm -rf osdev.img
	@rm -rf sysroot/drivers/*
	@rm -rf sysroot/programs/*.elf
	@$(MAKE) -C user clean
	@$(MAKE) -C drivers clean ARCH=$(ARCH)

run-x86_64:
	qemu-system-x86_64 -m 128M -M q35 -drive file=osdev.img,format=raw $(QARG) -serial stdio

run-x86_64-uefi:
	qemu-system-x86_64 -m 128M -M q35 -drive file=osdev.img,format=raw $(QARG) -serial stdio -drive if=pflash,unit=0,format=raw,file=edk2-ovmf/ovmf-code-$(ARCH).fd,readonly=on

run-x86_64-efi-dbg:
	qemu-system-x86_64 -m 128M -s -S -M q35 -drive file=osdev.img,format=raw $(QARG) -serial stdio -drive if=pflash,unit=0,format=raw,file=edk2-ovmf/ovmf-code-$(ARCH).fd,readonly=on