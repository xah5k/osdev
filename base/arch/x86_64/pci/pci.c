#include <arch/x86_64/pci/pci.h>
#include <arch/x86_64/cpu/paging.h>
#include <printfwrapper.h>
#include <mm/heap.h>
#include <kedriver.h>
#include <disk/ahci.h>
KePciDeviceHdr* gPciDevListHead = NULL;

KePciDeviceHdr* PciGetLinkedList() {
    return gPciDevListHead;
}
KE_EXPORT_SYMBOL(PciGetLinkedList);

uint32_t PciReadDword(uint16_t base, uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
    volatile uint32_t* p = PCI_ECAM(base, bus, dev, func, offset & ~0x3);
    volatile uint32_t* v = (volatile uint32_t*)P2V(p);
    return *v;
}
KE_EXPORT_SYMBOL(PciReadDword);

void PciWriteDword(uint16_t base, uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint32_t value) {
    volatile uint32_t* p = PCI_ECAM(base, bus, dev, func, offset & ~0x1);
    volatile uint32_t* v = (volatile uint32_t*)P2V(p);
    *v = value;
}

KE_EXPORT_SYMBOL(PciWriteDword);

void PciEnumFunction(uint64_t DevAddress, uint64_t Function, uint64_t Base, uint64_t Bus, uint64_t Dev) {
    uint64_t Offset = Function << 12;
    uint64_t FuncAddress = DevAddress + Offset;
    // map address otherwise mmu will go oh shittings
    MmuMapPage((pagetable*)_x86_64_get_pml4(), (physaddr)FuncAddress, (virtaddr)P2V(FuncAddress), MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_PCD);
    PciDeviceHeader* PciDevHdr = (PciDeviceHeader*)P2V(FuncAddress);
    if (PciDevHdr->DeviceID == 0) return;
    if (PciDevHdr->DeviceID == 0xFFFF) return;
    // printf("pci: new device: %lx:%lx\r\n", PciDevHdr->VendorID, PciDevHdr->DeviceID);
    KePciDeviceHdr* LinkedListType = MmAllocate(sizeof(KePciDeviceHdr));
    LinkedListType->Bus = Bus;
    LinkedListType->Dev = Dev;
    LinkedListType->Func = Function;
    LinkedListType->EcamBase = Base;
    LinkedListType->Header = PciDevHdr;
    LinkedListType->Next = gPciDevListHead;
    gPciDevListHead = LinkedListType;
    // directly initalize ahci for now
    if (PciDevHdr->Class == 0x01 && PciDevHdr->Subclass == 0x06 && PciDevHdr->ProgIf == 0x01) {
        KeDriverObj* Ahci = AhciInitalize(PciDevHdr);
        KSTATUS Init = Ahci->Initalize(Ahci);
        if (Init != KSUCCESS) {
            printf("pci: warn: failed to initalize ahci driver.\r\n");
            MmFree(Ahci);
        }
        KeDrvRegisterDriver(Ahci);
    }
}

void PciEnumDev(uint64_t BusAddress, uint64_t Device, uint64_t Base, uint64_t Bus) {
    uint64_t Offset = Device << 15;
    uint64_t DeviceAddress = BusAddress + Offset;
    // map address otherwise mmu will go oh shittings
    MmuMapPage((pagetable*)_x86_64_get_pml4(), (physaddr)DeviceAddress, (virtaddr)P2V(DeviceAddress), MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_PCD);
    PciDeviceHeader* PciDevHdr = (PciDeviceHeader*)P2V(DeviceAddress);
    if (PciDevHdr->DeviceID == 0) return;
    if (PciDevHdr->DeviceID == 0xFFFF) return;
    for (uint64_t Func = 0; Func < 8; Func++) {
        PciEnumFunction(DeviceAddress, Func, Base, Bus, Device);
    }
}

void PciEnumBus(uint64_t Base, uint64_t Bus) {
    uint64_t Offset = Bus << 20;
    uint64_t BusAddress = Base + Offset;
    // map address otherwise mmu will go oh shittings
    // note: we still in identity mapped so we dont need to convert pml4
    MmuMapPage((pagetable*)_x86_64_get_pml4(), (physaddr)BusAddress, (virtaddr)P2V(BusAddress), MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_PCD);
    PciDeviceHeader* PciDevHdr = (PciDeviceHeader*)P2V(BusAddress);
    if (PciDevHdr->DeviceID == 0 || PciDevHdr->DeviceID == 0xFFFF) return;
    for (uint64_t Dev = 0; Dev < 32; Dev++) {
        PciEnumDev(BusAddress, Dev, Base, Bus);
    }
}
void PciEnumerate(AcpiMcfgTable* mcfg) {
    uint64_t Entries = ((mcfg->header.Length) - sizeof(AcpiMcfgTable)) / sizeof(PciConfigSpBaStruct);
    // printf("pci: entries=%d\r\n", Entries);
    for (uint64_t i = 0; i < Entries; i++) {
        PciConfigSpBaStruct* DeviceConfig = (PciConfigSpBaStruct*)(((uint64_t)mcfg->Ecm) + (16 * i));
        for (uint64_t Bus = DeviceConfig->StartBus; Bus < DeviceConfig->EndBus; Bus++) {
            PciEnumBus(DeviceConfig->Base, Bus);
        }
    }
}