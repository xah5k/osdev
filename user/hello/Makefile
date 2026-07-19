PROGNAME = hello
all: compile

compile:
	$(ARCH)-elf-gcc -Wall -fno-pie -ffreestanding -fpic -mno-red-zone -fno-stack-protector -c hello.c -o hello.obj
	$(ARCH)-elf-ld -g -nostdlib -n -T hello.ld -no-pie hello.obj -o $(PROGNAME).elf
	echo [build program $(PROGNAME).elf]

cpsysroot:
	cp $(PROGNAME).elf ../../sysroot/programs/$(PROGNAME).elf

clean:
	rm -rf user.prog
	rm -rf $(PROGNAME).elf
	rm -rf hello.obj