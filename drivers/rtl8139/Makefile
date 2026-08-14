DRVNAME=rtl8139
CFLAGS = -g -Wall -ffreestanding -mno-red-zone -fno-stack-protector -nostdlib -I../../base -D__$(ARCH)__ -mcmodel=large -fno-pic
LDFLAGS = -g -nostdlib -n -T link.ld -r 
override SRCFILES := $(shell find -L drvbase -type f -not -path 'drvbase/arch/*' 2>/dev/null | LC_ALL=C sort)
override SRCFILES += $(shell find -L drvbase/arch/$(ARCH) -type f 2>/dev/null | LC_ALL=C sort)
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

all: $(DRVNAME).sys

$(DRVNAME).sys: $(OBJ)
	@mkdir -p "$(dir $@)"
	@x86_64-elf-ld $(LDFLAGS) $(OBJ) -o $@
	@echo "[build driver $(DRVNAME).sys] $<"

obj-$(ARCH)/%.c.o: %.c
	@mkdir -p "$(dir $@)"
	@$(ARCH)-elf-gcc $(CFLAGS) -c $< -o $@
	@echo "[build driver $(DRVNAME).sys] $<"

obj-x86_64/%.asm.o: %.asm
	@mkdir -p "$(dir $@)"
	@nasm -f elf64 $< -o $@
	@echo [build driver $(DRVNAME).sys]" $<"

cpsysroot:
	cp $(DRVNAME).sys ../../sysroot/drivers/$(DRVNAME).sys
clean:
	@rm -rf $(DRVNAME).sys
	@rm -rf obj-$(ARCH)