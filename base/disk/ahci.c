#include <disk/ahci.h>
#include <mm/heap.h>
#include <stddef.h>
#include <memory.h>
#include <mm/pmm.h>




static PciDeviceHeader* gPciBase;
static AhciHbaMemory* gAhciAbar;
static KeAhciPort* gKeAhciPorts[32];
static uint8_t gTotalPorts = 0;

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

void AhciPortStartCmd(KeAhciPort* Port) {
    while (Port->HbaPort->CmdStatus & 0x8000);
    Port->HbaPort->CmdStatus |= 0x0010;
    Port->HbaPort->CmdStatus |= 0x0001;

}
void AhciPortStopCmd(KeAhciPort* Port) {
    Port->HbaPort->CmdStatus &= ~0x0001;
    Port->HbaPort->CmdStatus &= ~0x0010;
    while (1) {
        if (Port->HbaPort->CmdStatus & 0x4000) {
            continue;
        }
        if (Port->HbaPort->CmdStatus & 0x8000) {
            continue;
        }
        break;
    }
}

KSTATUS AhciPortInitalize(KeAhciPort* Port) {
    AhciPortStopCmd(Port);
    void* NewBase = PmmAllocate();
    if (!NewBase) return KOOMERR;
    Port->HbaPort->CmdListBase = (uint32_t)((uint64_t)NewBase);
    Port->HbaPort->CmdListBaseUpper = (uint32_t)((uint64_t)NewBase >> 32);
    memset((void*)P2V(Port->HbaPort->CmdListBase), 0, 1024);
    void* FisBase = PmmAllocate();
    if (!FisBase) return KOOMERR;
    Port->HbaPort->FisBase = (uint32_t)((uint64_t)FisBase);
    Port->HbaPort->FisBaseUpper = (uint32_t)((uint64_t)FisBase >> 32);
    memset((void*)P2V(Port->HbaPort->FisBase), 0, 256);
    AhciHbaCmdHdr* CmdHdr = (AhciHbaCmdHdr*)((uint64_t)Port->HbaPort->CmdListBase + (uint64_t)(Port->HbaPort->CmdListBaseUpper << 32));
    for (int i = 0; i < 32; i++) {
        CmdHdr[i].PrdtLength = 8;
        void* CmdTableAddress = PmmAllocate();
        if (!CmdTableAddress) return KOOMERR;
        uint64_t Address = (uint64_t)CmdTableAddress + (i << 8);
        CmdHdr[i].CmdTableBase = (uint32_t)((uint64_t)Address);
        CmdHdr[i].CmdTableBaseUpper = (uint32_t)((uint64_t)Address >> 32);
        memset((void*)P2V(CmdTableAddress), 0, 256);
    }
    AhciPortStartCmd(Port);
    return KSUCCESS;
}

void AhciProbePorts() {
    uint32_t PortsImpl = gAhciAbar->PortsImpl;
    for (int i = 0; i < 32; i++) {
        if (PortsImpl & (1<<i)) {
            AhciHbaPort* Port = &gAhciAbar->Ports[i];
            AhciHbaPortType Type = AhciChkType(Port);
            if (Type == AHCI_TYPE_SATA || Type == AHCI_TYPE_SATAPI) {
                gKeAhciPorts[gTotalPorts] = MmAllocate(sizeof(KeAhciPort));
                gKeAhciPorts[gTotalPorts]->HbaPort = Port;
                gKeAhciPorts[gTotalPorts]->HbaType = Type;
                gKeAhciPorts[gTotalPorts]->PortIndex = gTotalPorts;
                gTotalPorts++;
            }
        }
    }
}

KeAhciPort* AhciGetPort(uint8_t Index) {
    return gKeAhciPorts[Index];
}

KSTATUS DriverEntry(KeDriverObj* Self) {
    KeDrvWriteFmt("ahci: DriverEntry.\r\n");
    KeDrvWriteFmt("ahci: gPciBase = 0x%lx\r\n", gPciBase);
    gAhciAbar = (AhciHbaMemory*)((PciDeviceHeaderTy0*)gPciBase)->BAR5;
    KeDrvWriteFmt("ahci: gAhciAbar = 0x%lx\r\n", gAhciAbar);
    // todo: dont identity map it for obvious reasons but temporarily for now
    MmuMapPage((pagetable*)_x86_64_get_pml4(), (virtaddr)gAhciAbar, (physaddr)gAhciAbar, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE | MMU_PAGE_BIT_PCD | MMU_PAGE_BIT_PWT);;
    AhciProbePorts();
    for (int i = 0; i < gTotalPorts; i++) {
        KeAhciPort* Port = gKeAhciPorts[i];
        AhciPortInitalize(Port);
    }
    return KSUCCESS;
}

KeDriverObj* AhciInitalize(PciDeviceHeader* PciBase) {
    KeDriverObj* driver = MmAllocate(sizeof(KeDriverObj));
    gPciBase = PciBase;
    if (!driver) return NULL;
    driver->Initalize = DriverEntry;
    memcpy(driver->Name, "ahci", 4);
}