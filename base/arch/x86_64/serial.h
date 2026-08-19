#pragma once
#include <stdint.h>


void InitSerialConsole(uint16_t port);
void WritecSerial(uint16_t port, char c);
void WriteSerial(uint16_t port, const char* string);