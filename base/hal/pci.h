#pragma once
#ifdef __x86_64__
#include <arch/x86_64/pci/pci.h>
#endif

typedef struct KePciDeviceHdr {
    uint64_t Bus;
    uint64_t Dev;
    uint64_t Func;
    uint64_t EcamBase;
    PciDeviceHeader* Header;
    struct KePciDeviceHdr* Next;
} KePciDeviceHdr; 
KePciDeviceHdr* PciGetLinkedList();
void PciAddDevice(uint8_t Bus, uint8_t Dev, uint8_t Function, uint64_t Base, PciDeviceHeader* PciDevHdr);