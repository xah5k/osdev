LIBOUT=libah5k.a
all: compile
CFLAGS=-Wall -ffreestanding -mno-red-zone -fno-stack-protector -nostdlib -D__$(ARCH)__

compile:
	x86_64-elf-gcc $(CFLAGS) -c crt0.c -o crt0.o
	nasm -f elf64 syscall.asm -o syscall.o
	x86_64-elf-ar rcs $(LIBOUT) crt0.o syscall.o

clean:
	rm -rf *.o
	rm -rf *.a