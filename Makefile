ARCH=x86_64
CFLAGS = -Wall -fno-pie -ffreestanding -fpic -mno-red-zone -fno-stack-protector -nostdlib -I./base -D__$(ARCH)__
LDFLAGS = -nostdlib -n -T link.ld -no-pie
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
all: preinit $(KNAME).x86_64.elf


produceimage:
	cp $(KNAME).$(ARCH).elf sysroot/boot/osdev.elf
	tools/mkbootimg bootimg.json osdev.img

# Fetch external sources (like `bootboot.h`)
preinit:
	@exec ./preinit.sh


$(KNAME).x86_64.elf: $(OBJ)
	@mkdir -p "$(dir $@)"
	@x86_64-elf-ld $(LDFLAGS) $(OBJ) -o $@
	@echo " $<"
	@make produceimage

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


run-x86_64:
	qemu-system-x86_64 osdev.img -M q35 $(QARG) -serial stdio

run-x86_64-uefi:
	qemu-system-x86_64 osdev.img -M q35 $(QARG) -serial stdio -drive if=pflash,unit=0,format=raw,file=edk2-ovmf/ovmf-code-$(ARCH).fd,readonly=on

run-x86_64-efi-dbg:
	qemu-system-x86_64 -s -S osdev.img -M q35 $(QARG) -serial stdio -drive if=pflash,unit=0,format=raw,file=edk2-ovmf/ovmf-code-$(ARCH).fd,readonly=on