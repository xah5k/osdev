#pragma once
#include <stdint.h>

void outb(uint16_t port, char value);
uint8_t inb(uint16_t port);
