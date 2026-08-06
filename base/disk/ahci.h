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

typedef enum {
    AHCI_FIS_TYPE_RGH2D = 0x27,
    AHCI_FIS_TYPE_RGD2H = 0x34,
    AHCI_FIS_TYPE_DMA_ACT = 0x39,
    AHCI_FIS_TYPE_DMA_SETUP = 0x41,
    AHCI_FIS_TYPE_DATA = 0x46,
    AHCI_FIS_TYPE_BIST = 0x58,
    AHCI_FIS_TYPE_PIO_SETUP = 0x5F,
    AHCI_FIS_TYPE_DEV_BITS = 0xA1
} AhciFisType;

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
    uint32_t Rsv1[4];
} AhciHbaCmdHdr;

typedef struct {
    uint8_t FisType;
    uint8_t PmportAndC;
    uint8_t Cmd;
    uint8_t FeatureLow;
    uint8_t Lba0;
    uint8_t Lba1;
    uint8_t Lba2;
    uint8_t DevRegister;
    uint8_t Lba3;
    uint8_t Lba4;
    uint8_t Lba5;
    uint8_t FeatureHigh;
    uint8_t CountLow;
    uint8_t CountHigh;
    uint8_t IsoCmdCompletion;
    uint8_t Ctrl;
    uint8_t Rsv1[4];
} AhciFisRegH2d;

typedef struct {
    uint32_t DataBase;
    uint32_t DataBaseUpper;
    uint32_t Rsv0;
    uint32_t ByteCount : 22;
    uint32_t Rsv1 : 9;
    uint32_t IntOnCompl : 1;
} AhciHbaPrdtEntry;

typedef struct {
    uint8_t CmdFis[64];
    uint8_t AtApiCmd[16];
    uint8_t Rsv0[48];
    AhciHbaPrdtEntry PrdtEntry[];
} AhciHbaCmdTable;

// kernel specific bookkeeping
typedef struct {
    AhciHbaPort* HbaPort;
    AhciHbaPortType HbaType;
    uint8_t* Buffer;
    uint8_t PortIndex;
} KeAhciPort;

KSTATUS AhciPortRead(KeAhciPort* Port, uint64_t Sector, uint32_t SectorCount, void* Buffer);
KeAhciPort* AhciGetPort(uint8_t Index); 
KeDriverObj* AhciInitalize(PciDeviceHeader* PciBase);
