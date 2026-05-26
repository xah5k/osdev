#include "serial.h"
#if __ARCH__ == x86_64
#include "arch/x86_64/ports.h"
#endif

void InitSerialConsole(uint16_t port) {
    outb(port + 1, 0x00);
    outb(port + 3, 0x80);
    outb(port + 0, 0x03);
    outb(port + 1, 0x00);
    outb(port + 3, 0x03);
    outb(port + 2, 0xC7);
    outb(port + 4, 0x0B);
    outb(port + 4, 0x1E);
    outb(port + 0, 0xAE);

    if (inb(port + 0) != 0xAE) {
        return;
    }

    outb(port + 4, 0x0f);
}

void WritecSerial(uint16_t port, char c) {
    while ((inb(port + 5) & 0x20) == 0);

    outb(port, c);
}

void WriteSerial(uint16_t port, const char* string) {
    while (*string != '\0') {
        WritecSerial(port, *string);
        string++;
    }
}