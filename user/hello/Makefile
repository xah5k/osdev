PROGNAME=hello

all: compile

compile:
	x86_64-ah5kos-gcc -g src/hello.c -o $(PROGNAME).elf
cpsysroot:
	cp $(PROGNAME).elf ../../sysroot/programs/$(PROGNAME).elf
clean:
	@rm -rf $(PROGNAME).elf
	@rm -rf obj-$(ARCH)