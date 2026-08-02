#pragma once
#include <stdint.h>

typedef struct {
    char Signature[4];
    uint32_t Length;
    char Revision;
    char Checksum;
    char OemId[6];
    uint64_t OemTableId;
    char OemRevision[4];
    char CreatorId[4];
    char CreatorRevision[4];
} __attribute__((packed)) AcpiTableHeader;
typedef struct {
    AcpiTableHeader header;
    uint64_t Tables[];
} __attribute__((packed)) AcpiXsdtTable;

typedef struct {
    AcpiTableHeader header;
    uint32_t Tables[];
} __attribute__((packed)) AcpiRsdtTable;

typedef struct {
    char Type;
    char Length;
} __attribute__((packed)) AcpiMadtIntDeviceHdr;

#define ACPI_MADT_FLAG_PROCESSOR_ENABLED (1<<0) 
#define ACPI_MADT_FLAG_PROCESSOR_CAPABLE (1<<1)

typedef struct {
    AcpiMadtIntDeviceHdr header;
    char ProcessorId;
    char ApicId;
    uint32_t Flags;
} __attribute__((packed)) AcpiMadtIntDevLapic; // processor local apic

typedef struct {
    AcpiMadtIntDeviceHdr header;
    char IoApicId;
    char none;
    uint32_t IoApicAddress;
    uint32_t GsiBase;
} __attribute__((packed)) AcpiMadtIntDevIoApic;

typedef struct {
    AcpiTableHeader header;
    uint32_t LapicAddress;
    uint32_t Flags;
    AcpiMadtIntDeviceHdr IntDevices[];
} __attribute__((packed)) AcpiMadtTable;

typedef struct {
    uint64_t Base;
    uint16_t SegGroupNumber;
    uint8_t StartBus;
    uint8_t EndBus;
    uint32_t Reserved;
} __attribute__((packed)) PciConfigSpBaStruct;

typedef struct {
    AcpiTableHeader header;
    uint64_t Reserved;
    PciConfigSpBaStruct Ecm[];
} __attribute__((packed)) AcpiMcfgTable;

void* AcpiFindTable(AcpiRsdtTable* rsdt, char* signature); 