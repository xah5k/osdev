all: compile

compile:
	gcc -Wall -fno-pie -ffreestanding -fpic -mno-red-zone -fno-stack-protector -c hello.c -o hello.obj
	

clean:
	rm -rf user.prog
	rm -rf hello.elf