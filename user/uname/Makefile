PROGNAME=uname

all: compile

compile:
	x86_64-ah5kos-gcc src/uname.c -o $(PROGNAME).elf
cpsysroot:
	cp $(PROGNAME).elf ../../sysroot/programs/$(PROGNAME).elf
clean:
	@rm -rf $(PROGNAME).elf
	@rm -rf obj-$(ARCH)