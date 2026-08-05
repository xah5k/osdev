#pragma once
#include <arch/x86_64/pci/pci.h>
#include <kedriver.h>
typedef enum {
    AHCI_TYPE_NONE,
    AHCI_TYPE_SATA,
    AHCI_TYPE_SEMB,
    AHCI_TYPE_PM,
    AHCI_TYPE_SATAPI
} AhciHbaPortType;

typedef struct {
    uint32_t CmdListBase;
    uint32_t CmdListBaseUpper;
    uint32_t FisBase;
    uint32_t FisBaseUpper;
    uint32_t InterruptStatus;
    uint32_t InterruptEnable;
    uint32_t CmdStatus;
    uint32_t Rsv0;
    uint32_t TaskFileData;
    uint32_t Signature;
    uint32_t SataStatus;
    uint32_t SataCtrl;
    uint32_t SataError;
    uint32_t SataActive;
    uint32_t CmdIssue;
    uint32_t SataNotification;
    uint32_t FisSwitchCtrl;
    uint32_t Rsv1[11];
    uint32_t Vendor[4];
} __attribute__((packed)) AhciHbaPort;

typedef struct {
    uint32_t HostCaps;
    uint32_t GlobalHostCtrl;
    uint32_t InterruptStatus;
    uint32_t PortsImpl;
    uint32_t Version;
    uint32_t CccCtrl;
    uint32_t CccPorts;
    uint32_t EnclosureMgmtLoc;
    uint32_t EnclosureMgmtCtrl;
    uint32_t Cap2;
    uint32_t FwHandoffCtrlStatus;
    uint8_t Rsv0[116];
    uint8_t Vendor[96];
    AhciHbaPort Ports[32];
} __attribute__((packed)) AhciHbaMemory;

typedef struct {
    uint8_t CommandFisLength : 5;
    uint8_t AtApi : 1;
    uint8_t Write : 1;
    uint8_t Prefetch : 1;
    uint8_t Reset : 1;
    uint8_t bIst : 1;
    uint8_t ClearBusy : 1;
    uint8_t Rsv0 : 1;
    uint8_t PortMultiplier : 4;
    uint16_t PrdtLength;
    uint32_t PrdtCount;
    uint32_t CmdTableBase;
    uint32_t CmdTableBaseUpper;
} AhciHbaCmdHdr;

// kernel specific bookkeeping
typedef struct {
    AhciHbaPort* HbaPort;
    AhciHbaPortType HbaType;
    uint8_t* Buffer;
    uint8_t PortIndex;
} KeAhciPort;

KeAhciPort* AhciGetPort(uint8_t Index); 
KeDriverObj* AhciInitalize(PciDeviceHeader* PciBase);
