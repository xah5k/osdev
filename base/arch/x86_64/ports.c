#include "ports.h"
#include <kedriver.h>
void outb(uint16_t port, char value) {
    __asm__ volatile ("outb %b0, %w1" : : "a"(value), "Nd"(port) : "memory");
}
KE_EXPORT_SYMBOL(outb);

uint8_t inb(uint16_t port) {
    char out;
    __asm__ volatile ("inb %1, %b0" : "=a"(out) : "Nd"(port) : "memory");
    return out;
}
KE_EXPORT_SYMBOL(inb);