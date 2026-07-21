PROGNAME=hello
CFLAGS = -g -Wall -ffreestanding -mno-red-zone -fno-stack-protector -nostdlib -D__$(ARCH)__
LDFLAGS = -g -nostdlib -n -T link.ld
override SRCFILES := $(shell find -L src -type f 2>/dev/null | LC_ALL=C sort)
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

all: $(PROGNAME).elf

$(PROGNAME).elf: $(OBJ)
	@mkdir -p "$(dir $@)"
	@x86_64-elf-ld $(LDFLAGS) $(OBJ) -o $@
	@echo [build program $(PROGNAME).elf]" $<"

obj-$(ARCH)/%.c.o: %.c
	@mkdir -p "$(dir $@)"
	@$(ARCH)-elf-gcc $(CFLAGS) -c $< -o $@
	@echo [build program $(PROGNAME).elf]" $<"

obj-x86_64/%.asm.o: %.asm
	@mkdir -p "$(dir $@)"
	@nasm -f elf64 $< -o $@
	@echo [build program $(PROGNAME).elf]" $<"

cpsysroot:
	cp $(PROGNAME).elf ../../sysroot/programs/$(PROGNAME).elf
clean:
	@rm -rf $(PROGNAME).elf
	@rm -rf obj-$(ARCH)