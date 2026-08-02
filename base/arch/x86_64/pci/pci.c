#include <arch/x86_64/pci/pci.h>
#include <arch/x86_64/cpu/paging.h>
#include <printfwrapper.h>
#include <mm/heap.h>
KePciDeviceHdr* gPciDevListHead = NULL;

KePciDeviceHdr* PciGetLinkedList() {
    return gPciDevListHead;
}

void PciEnumFunction(uint64_t DevAddress, uint64_t Function) {
    uint64_t Offset = Function << 12;
    uint64_t FuncAddress = DevAddress + Offset;
    // map address otherwise mmu will go oh shittings
    MmuMapPage((pagetable*)_x86_64_get_pml4(), (physaddr)FuncAddress, (virtaddr)P2V(FuncAddress), MMU_PAGE_BIT_P_PRESENT);
    PciDeviceHeader* PciDevHdr = (PciDeviceHeader*)P2V(FuncAddress);
    if (PciDevHdr->DeviceID == 0) return;
    if (PciDevHdr->DeviceID == 0xFFFF) return;
    printf("pci: new device: %lx:%lx\r\n", PciDevHdr->VendorID, PciDevHdr->DeviceID);
    KePciDeviceHdr* LinkedListType = MmAllocate(sizeof(KePciDeviceHdr));
    LinkedListType->Header = PciDevHdr;
    LinkedListType->Next = gPciDevListHead;
    gPciDevListHead = LinkedListType;
}

void PciEnumDev(uint64_t BusAddress, uint64_t Device) {
    uint64_t Offset = Device << 15;
    uint64_t DeviceAddress = BusAddress + Offset;
    // map address otherwise mmu will go oh shittings
    MmuMapPage((pagetable*)_x86_64_get_pml4(), (physaddr)DeviceAddress, (virtaddr)P2V(DeviceAddress), MMU_PAGE_BIT_P_PRESENT);
    PciDeviceHeader* PciDevHdr = (PciDeviceHeader*)P2V(DeviceAddress);
    if (PciDevHdr->DeviceID == 0) return;
    if (PciDevHdr->DeviceID == 0xFFFF) return;
    for (uint64_t Func = 0; Func < 8; Func++) {
        PciEnumFunction(DeviceAddress, Func);
    }
}

void PciEnumBus(uint64_t Base, uint64_t Bus) {
    uint64_t Offset = Bus << 20;
    uint64_t BusAddress = Base + Offset;
    // map address otherwise mmu will go oh shittings
    MmuMapPage((pagetable*)_x86_64_get_pml4(), (physaddr)BusAddress, (virtaddr)P2V(BusAddress), MMU_PAGE_BIT_P_PRESENT);
    PciDeviceHeader* PciDevHdr = (PciDeviceHeader*)P2V(BusAddress);
    if (PciDevHdr->DeviceID == 0 || PciDevHdr->DeviceID == 0xFFFF) return;
    for (uint64_t Dev = 0; Dev < 32; Dev++) {
        PciEnumDev(BusAddress, Dev);
    }
}
void PciEnumerate(AcpiMcfgTable* mcfg) {
    uint64_t Entries = ((mcfg->header.Length) - sizeof(AcpiMcfgTable)) / sizeof(PciConfigSpBaStruct);
    printf("pci: entries=%d\r\n", Entries);
    for (uint64_t i = 0; i < Entries; i++) {
        PciConfigSpBaStruct* DeviceConfig = (PciConfigSpBaStruct*)(((uint64_t)mcfg->Ecm) + (16 * i));
        for (uint64_t Bus = DeviceConfig->StartBus; Bus < DeviceConfig->EndBus; Bus++) {
            PciEnumBus(DeviceConfig->Base, Bus);
        }
    }
}