#pragma once
#include <arch/x86_64/acpi.h>
#include <arch/x86_64/cpu/paging.h>
void CpuInitalizeIoApic(AcpiRsdtTable* rsdt);
void CpuIoApicSetRedirEntry(uint8_t gsi, uint64_t data) ;
virtaddr CpuGetIoApicVirtBase() ;