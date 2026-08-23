#pragma once
#include <stdint.h>
#include <kstatus.h>

#include <hal/acpi.h>

void* AcpiFindTable(AcpiRsdtTable* rsdt, char* signature); 
KSTATUS AcpiSystemShutdown();
KSTATUS AcpiResolvePciGsi();
uint32_t AcpiPciGsiLookup(uint64_t Bus, uint64_t Dev, uint8_t Pin);