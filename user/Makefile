all: compile

compile:
	x86_64-elf-gcc -Wall -fno-pie -ffreestanding -fpic -mno-red-zone -fno-stack-protector -c hello.c -o hello.obj
	x86_64-elf-ld -g -nostdlib -n -T hello.ld -no-pie hello.obj -o hello.elf

clean:
	rm -rf user.prog
	rm -rf hello.elf