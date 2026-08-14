#pragma once
#include <stdint.h>

void outb(uint16_t port, char value);
uint8_t inb(uint16_t port);
void outl(uint16_t port, uint32_t value);
uint32_t inl(uint16_t port);
void outw(uint16_t port, uint16_t value);
uint16_t inw(uint16_t port);