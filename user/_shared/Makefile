OUT=libdwmapi.a
OUT2=libosui.a
all: compile compile2
CFLAGS=-Wall -Wextra

compile:
	x86_64-ah5kos-gcc $(CFLAGS) -c dwmapi.c -o dwmapi.o
	x86_64-ah5kos-ar rcs $(OUT) dwmapi.o

compile2:
	x86_64-ah5kos-gcc $(CFLAGS) -c osui/btn.c -o btn.o
	x86_64-ah5kos-gcc $(CFLAGS) -c osui/osui.c -o osui.o
	x86_64-ah5kos-ar rcs $(OUT2) osui.o btn.o

clean:
	rm -rf *.o
	rm -rf *.a