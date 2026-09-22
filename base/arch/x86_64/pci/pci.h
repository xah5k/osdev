#pragma once
#include <stdint.h>
#include "../acpi.h"


typedef struct {
    uint16_t VendorID;
    uint16_t DeviceID;
    uint16_t CommandReg;
    uint16_t StatusReg;
    uint8_t RevisionID;
    uint8_t ProgIf;
    uint8_t Subclass;
    uint8_t Class;
    uint8_t CacheLineSz;
    uint8_t LatencyTimer;
    uint8_t HeaderType;
    uint8_t Bist;
} PciDeviceHeader;

typedef struct {
    PciDeviceHeader Hdr;
    uint32_t BAR0;
    uint32_t BAR1;
    uint32_t BAR2;
    uint32_t BAR3;
    uint32_t BAR4;
    uint32_t BAR5;
    uint32_t CardbusCISPtr;
    uint16_t SbSysVendorId;
    uint16_t SbSysId;
    uint32_t ExpRomBase;
    uint8_t CapPtr;
    uint8_t Rsv0;
    uint16_t Rsv1;
    uint32_t Rsv2;
    uint8_t InterruptLine;
    uint8_t InterruptPin;
    uint8_t MaxGrant;
    uint8_t MaxTime;
} PciDeviceHeaderTy0;

void PciEnumerate(AcpiMcfgTable* mcfg);
void PciWriteDword(uint16_t base, uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint32_t value);
uint32_t PciReadDword(uint16_t base, uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);

#define PCI_ECAM(base, bus, dev, func, offset) ((volatile uint32_t*)((uint64_t)(base) + ((uint64_t)(bus)  << 20) + ((uint64_t)(dev)  << 15) + ((uint64_t)(func) << 12) + (uint64_t)(offset)))