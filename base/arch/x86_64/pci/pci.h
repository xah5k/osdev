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
    // todo based on HeaderType add more so we can also get
    // BARx addresses.
} PciDeviceHeader;

typedef struct KePciDeviceHdr {
    PciDeviceHeader* Header;
    struct KePciDeviceHdr* Next;
} KePciDeviceHdr; 

void PciEnumerate(AcpiMcfgTable* mcfg);
KePciDeviceHdr* PciGetLinkedList();