#include "pci.h"
#include <stddef.h>
#include <kedriver.h>
#include <mm/heap.h>
KePciDeviceHdr* gPciDevListHead = NULL;

KePciDeviceHdr* PciGetLinkedList() {
    return gPciDevListHead;
}
KE_EXPORT_SYMBOL(PciGetLinkedList);

void PciAddDevice(uint8_t Bus, uint8_t Dev, uint8_t Function, uint64_t Base, PciDeviceHeader* PciDevHdr) {
    KePciDeviceHdr* LinkedListType = MmAllocate(sizeof(KePciDeviceHdr));
    LinkedListType->Bus = Bus;
    LinkedListType->Dev = Dev;
    LinkedListType->Func = Function;
    LinkedListType->EcamBase = Base;
    LinkedListType->Header = PciDevHdr;
    LinkedListType->Next = gPciDevListHead;
    gPciDevListHead = LinkedListType;
}