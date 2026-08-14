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


void outl(uint16_t port, uint32_t value) {
    __asm__ volatile ("outl %k0, %1" : : "a"(value), "Nd"(port) : "memory");
}
KE_EXPORT_SYMBOL(outl);

uint32_t inl(uint16_t port) {
    uint32_t out;
    __asm__ volatile ("inl %1, %k0" : "=a"(out) : "Nd"(port) : "memory");
    return out;
}
KE_EXPORT_SYMBOL(inl);

void outw(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %w0, %1" : : "a"(value), "Nd"(port) : "memory");
}
KE_EXPORT_SYMBOL(outw);

uint16_t inw(uint16_t port) {
    uint16_t out;
    __asm__ volatile ("inw %1, %w0" : "=a"(out) : "Nd"(port) : "memory");
    return out;
}
KE_EXPORT_SYMBOL(inw);
