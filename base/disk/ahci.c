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
    AhciHbaCmdHdr* CmdHdr = (AhciHbaCmdHdr*)((uint64_t)Port->HbaPort->CmdListBase + (uint64_t)((uint64_t)Port->HbaPort->CmdListBaseUpper << 32));
    for (int i = 0; i < 32; i++) {
        CmdHdr[i].PrdtLength = 8;
        void* CmdTableAddress = PmmAllocate();
        if (!CmdTableAddress) return KOOMERR;
        uint64_t Address = (uint64_t)CmdTableAddress;
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

KSTATUS AhciPortRead(KeAhciPort* Port, uint64_t Sector, uint32_t SectorCount, void* Buffer) {
    uint32_t SectorLow = (uint32_t)Sector;
    uint32_t SectorHigh = (uint32_t)(Sector >> 32);
    Port->HbaPort->InterruptStatus = (uint32_t)-1;
    AhciHbaCmdHdr* CmdHdr = (AhciHbaCmdHdr*)P2V(((uint64_t)Port->HbaPort->CmdListBase + (uint64_t)((uint64_t)Port->HbaPort->CmdListBaseUpper << 32)));
    CmdHdr->CommandFisLength = sizeof(AhciFisRegH2d) / sizeof(uint32_t);
    CmdHdr->Write = 0;
    CmdHdr->PrdtLength = 1;
    AhciHbaCmdTable* CmdTable = (AhciHbaCmdTable*)P2V(CmdHdr->CmdTableBase);
    memset((void*)CmdTable, 0, sizeof(AhciHbaCmdTable) + (CmdHdr->PrdtLength - 1) * sizeof(AhciHbaPrdtEntry));
    CmdTable->PrdtEntry[0].DataBase = (uint32_t)((uint64_t)Buffer);
    CmdTable->PrdtEntry[0].DataBaseUpper = (uint32_t)((uint64_t)Buffer >> 32);
    CmdTable->PrdtEntry[0].ByteCount = (SectorCount << 9) - 1;
    CmdTable->PrdtEntry[0].IntOnCompl = 1;
    AhciFisRegH2d* CmdFis = (AhciFisRegH2d*)(&CmdTable->CmdFis);
    CmdFis->FisType = AHCI_FIS_TYPE_RGH2D;
    CmdFis->PmportAndC = 0x80;
    CmdFis->Cmd = 0x25;

    CmdFis->Lba0 = (uint8_t)SectorLow;
    CmdFis->Lba1 = (uint8_t)(SectorLow >> 8);
    CmdFis->Lba2 = (uint8_t)(SectorLow >> 16);
    CmdFis->Lba3 = (uint8_t)SectorHigh;
    CmdFis->Lba4 = (uint8_t)(SectorHigh >> 8);
    CmdFis->Lba5 = (uint8_t)(SectorHigh >> 16);

    CmdFis->DevRegister = 1<<6;
    CmdFis->CountLow = SectorCount & 0xFF;
    CmdFis->CountHigh = (SectorCount >> 8) & 0xFF;
    uint64_t SpinTimer = 0;
    while ((Port->HbaPort->TaskFileData & (0x80 | 0x08)) && SpinTimer < 100000) {
        SpinTimer++;
    }
    if (SpinTimer == 100000) {
        return KHUNG;
    }
    KeDrvWriteFmt("ahci: pre-issue CI=%08x TFD=%08x\r\n", Port->HbaPort->CmdIssue, Port->HbaPort->TaskFileData);
    Port->HbaPort->CmdIssue = 1;
    SpinTimer = 0;
    while (SpinTimer < 100000) {
        if (Port->HbaPort->CmdIssue == 0) break;
        if (Port->HbaPort->CmdIssue & (1 << 30)) {
            return KFAIL;
        }
        SpinTimer++;
    }
    if (SpinTimer == 100000) {
        KeDrvWriteFmt("ahci: port hung/timed out. CI=%08x TFD=%08x SERR=%08x IS=%08x\r\n", Port->HbaPort->CmdIssue, Port->HbaPort->TaskFileData, Port->HbaPort->SataError, Port->HbaPort->InterruptStatus);
        return KHUNG;
    }
    return KSUCCESS;
}

KSTATUS AhciPortWrite(KeAhciPort* Port, uint64_t Sector, uint32_t SectorCount, const void* Buffer) {
    uint32_t SectorLow = (uint32_t)Sector;
    uint32_t SectorHigh = (uint32_t)(Sector >> 32);
    Port->HbaPort->InterruptStatus = (uint32_t)-1;
    AhciHbaCmdHdr* CmdHdr = (AhciHbaCmdHdr*)P2V(((uint64_t)Port->HbaPort->CmdListBase + (uint64_t)((uint64_t)Port->HbaPort->CmdListBaseUpper << 32)));
    CmdHdr->CommandFisLength = sizeof(AhciFisRegH2d) / sizeof(uint32_t);
    CmdHdr->Write = 1;
    CmdHdr->PrdtLength = 1;
    AhciHbaCmdTable* CmdTable = (AhciHbaCmdTable*)P2V(CmdHdr->CmdTableBase);
    memset((void*)CmdTable, 0, sizeof(AhciHbaCmdTable) + (CmdHdr->PrdtLength - 1) * sizeof(AhciHbaPrdtEntry));
    CmdTable->PrdtEntry[0].DataBase = (uint32_t)((uint64_t)Buffer);
    CmdTable->PrdtEntry[0].DataBaseUpper = (uint32_t)((uint64_t)Buffer >> 32);
    CmdTable->PrdtEntry[0].ByteCount = (SectorCount << 9) - 1;
    CmdTable->PrdtEntry[0].IntOnCompl = 1;
    AhciFisRegH2d* CmdFis = (AhciFisRegH2d*)(&CmdTable->CmdFis);
    CmdFis->FisType = AHCI_FIS_TYPE_RGH2D;
    CmdFis->PmportAndC = 0x80;
    CmdFis->Cmd = 0xCA;

    CmdFis->Lba0 = (uint8_t)SectorLow;
    CmdFis->Lba1 = (uint8_t)(SectorLow >> 8);
    CmdFis->Lba2 = (uint8_t)(SectorLow >> 16);
    CmdFis->Lba3 = (uint8_t)SectorHigh;
    CmdFis->Lba4 = (uint8_t)(SectorHigh >> 8);
    CmdFis->Lba5 = (uint8_t)(SectorHigh >> 16);

    CmdFis->DevRegister = 1<<6;
    CmdFis->CountLow = SectorCount & 0xFF;
    CmdFis->CountHigh = (SectorCount >> 8) & 0xFF;
    uint64_t SpinTimer = 0;
    while ((Port->HbaPort->TaskFileData & (0x80 | 0x08)) && SpinTimer < 100000) {
        SpinTimer++;
    }
    if (SpinTimer == 100000) {
        return KHUNG;
    }
    KeDrvWriteFmt("ahci: pre-issue CI=%08x TFD=%08x\r\n", Port->HbaPort->CmdIssue, Port->HbaPort->TaskFileData);
    Port->HbaPort->CmdIssue = 1;
    SpinTimer = 0;
    while (SpinTimer < 100000) {
        if (Port->HbaPort->CmdIssue == 0) break;
        if (Port->HbaPort->CmdIssue & (1 << 30)) {
            return KFAIL;
        }
        SpinTimer++;
    }
    if (SpinTimer == 100000) {
        KeDrvWriteFmt("ahci: port hung/timed out. CI=%08x TFD=%08x SERR=%08x IS=%08x\r\n", Port->HbaPort->CmdIssue, Port->HbaPort->TaskFileData, Port->HbaPort->SataError, Port->HbaPort->InterruptStatus);
        return KHUNG;
    }
    return KSUCCESS;
}

KSTATUS DriverEntry(KeDriverObj* Self) {
    KeDrvWriteFmt("ahci: DriverEntry.\r\n");
    KeDrvWriteFmt("ahci: gPciBase = 0x%lx\r\n", gPciBase);
    gAhciAbar = (AhciHbaMemory*)(((PciDeviceHeaderTy0*)gPciBase)->BAR5 & ~0xFULL);
    KeDrvWriteFmt("ahci: gAhciAbar = 0x%lx\r\n", gAhciAbar);
    MmuMapPage((pagetable*)_x86_64_get_pml4(), (virtaddr)P2V(gAhciAbar), (physaddr)gAhciAbar, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE | MMU_PAGE_BIT_PCD | MMU_PAGE_BIT_PWT);;
    gAhciAbar = (AhciHbaMemory*)((uint64_t)gAhciAbar + gMmuVOffset);
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
    return driver;
}