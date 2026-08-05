#include <disk/ahci.h>
#include <mm/heap.h>
#include <stddef.h>
#include <memory.h>

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

static PciDeviceHeader* gPciBase;
static AhciHbaMemory* gAhciAbar;

AhciHbaPortType AhciChkType(AhciHbaPort* Port) {
    uint8_t Ipm = (Port->SataStatus >> 8) & 0b111;
    uint8_t DevDetection = (Port->SataStatus) & 0b111;
    if (DevDetection != 0x3) return AHCI_TYPE_NONE;
    if (Ipm != 0x1) return AHCI_TYPE_NONE;
    switch (Port->Signature) {
        case 0xEB140101: {
            return AHCI_TYPE_SATAPI;
        }
        case 0x00000101: {
            return AHCI_TYPE_SATA;
        }
        case 0x96690101: {
            return AHCI_TYPE_PM;
        }
        case 0xC33C0101: {
            return AHCI_TYPE_SEMB;
        }
        default: {
            return AHCI_TYPE_NONE;
        }
    }
    return AHCI_TYPE_NONE;
}
void AhciProbePorts() {
    uint32_t PortsImpl = gAhciAbar->PortsImpl;
    for (int i = 0; i < 32; i++) {
        if (PortsImpl & (1<<i)) {
            AhciHbaPort* Port = &gAhciAbar->Ports[i];
            AhciHbaPortType Type = AhciChkType(Port);
            if (Type == AHCI_TYPE_SATA) {
                KeDrvWriteFmt("ahci: detected sata drive.\r\n");
            } else if (Type == AHCI_TYPE_SATAPI) {
                KeDrvWriteFmt("ahci: detected satapi drive.\r\n");
            } else {
                KeDrvWriteFmt("ahci: detected not supported drive.\r\n");
            }
        }
    }
}
KSTATUS DriverEntry(KeDriverObj* Self) {
    KeDrvWriteFmt("ahci: DriverEntry.\r\n");
    KeDrvWriteFmt("ahci: gPciBase = 0x%lx\r\n", gPciBase);
    gAhciAbar = (AhciHbaMemory*)((PciDeviceHeaderTy0*)gPciBase)->BAR5;
    KeDrvWriteFmt("ahci: gAhciAbar = 0x%lx\r\n", gAhciAbar);
    // todo: dont identity map it for obvious reasons but temporarily for now
    MmuMapPage((pagetable*)_x86_64_get_pml4(), (virtaddr)gAhciAbar, (physaddr)gAhciAbar, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE | MMU_PAGE_BIT_PCD | MMU_PAGE_BIT_PWT);;
    AhciProbePorts();
    return KSUCCESS;
}

KeDriverObj* AhciInitalize(PciDeviceHeader* PciBase) {
    KeDriverObj* driver = MmAllocate(sizeof(KeDriverObj));
    gPciBase = PciBase;
    if (!driver) return NULL;
    driver->Initalize = DriverEntry;
    memcpy(driver->Name, "ahci", 4);
}