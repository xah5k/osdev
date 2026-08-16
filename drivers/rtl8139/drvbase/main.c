#include <kernel.h>
#include <kedriver.h>
#include <arch/x86_64/pci/pci.h>
#include <arch/x86_64/cpu/lapic.h>
#include <arch/x86_64/cpu/ioapic.h>
#include <arch/x86_64/acpi.h>
#include <arch/x86_64/cpu/idt.h>
#include <fs/vfs.h>
#include <mm/heap.h>
#include <mm/pmm.h>
#include <net/net.h>
#include <arch/x86_64/ports.h>
#include <util/util.h>
#include <memory.h>

#define RTL8139_RXBUF_SZ 8192+16+1500
#define RTL8139_INT_VECTOR 0x30

void Rtl8139InterruptHandler(CpuInterruptArgs* r) {
    KeDrvWrite("rtl8139: hello from interrupt handler!\r\n");
    CpuLapicEoi();
}

KSTATUS DriverEntry(KeDriverObj* Self) {
    memcpy(Self->Name, "rtl8139-nic", 12);
    KeDeviceObj* device = MmAllocate(sizeof(KeDeviceObj));
    memcpy(device->Name, "rtl8139", 8);
    device->Owner = Self;
    device->Device = NULL;
    device->Next = NULL;

    // look for device in pci list
    KePciDeviceHdr* PciDev = PciGetLinkedList();
    while (PciDev != NULL) {
        if (PciDev->Header->VendorID == 0x10ec && PciDev->Header->DeviceID == 0x8139) {;
            break; // found
        }
        PciDev = PciDev->Next;
    }
    if (PciDev == NULL) {
        // no rtl8139 in the system
        MmFree(device);
        return KUNSUPPORTED;
    }
    KeRegisterDevice(device);
    NetInterface* Nic = MmAllocate(sizeof(NetInterface));
    memcpy(Nic->Name, "rtl8139_eth0", 13);
    Nic->Device = device;
    Nic->IoBase = ((PciDeviceHeaderTy0*)PciDev->Header)->BAR0 & ~0x3;
    uint32_t Cmd = PciReadDword(PciDev->EcamBase, PciDev->Bus, PciDev->Dev, PciDev->Func, 0x04);
    Cmd |= (1 << 2); // dma
    Cmd &= ~(1 << 10); // if for some reason interrupt disable bit is 1 clear it
    PciWriteDword(PciDev->EcamBase, PciDev->Bus, PciDev->Dev, PciDev->Func, 0x04, Cmd);
    outb(Nic->IoBase + 0x52, 0); // turn on the rtl8139
    // reset (similar to ne2k)
    outb(Nic->IoBase + 0x37, 0x10);
    while ((inb(Nic->IoBase + 0x37) & 0x10) != 0);
    // setup receive buffer
    Nic->RxBuffer = (physaddr)PmmAllocatePages(UTIL_DIV_RUP(RTL8139_RXBUF_SZ, MMU_PAGE_SIZE));
    // we can only hope that PmmAllocatePages returns a number below 32bit limit (which it should cuz we're early in boot and there's no reason
    // for kernel to be using that much ram this early)
    outl(Nic->IoBase + 0x30, (uint32_t)Nic->RxBuffer);
    outw(Nic->IoBase + 0x3C, 0x0005);
    outl(Nic->IoBase + 0x44, 0xF | (1 << 7));
    // enable receive and transmit
    outb(Nic->IoBase + 0x37, 0x0C);
    // read mac address
    uint8_t Mac[6];
    for (uint16_t i = 0; i < 6; i++) {
        Mac[i] = inb(Nic->IoBase + i);
    }
    memcpy((void*)Nic->MacAddress, Mac, 6);
    uint32_t Gsi = AcpiPciGsiLookup(PciDev->Bus, PciDev->Dev, ((PciDeviceHeaderTy0*)PciDev->Header)->InterruptPin);
    uint64_t entry = 0;
    uint64_t dest = CpuLapicGetId();
    entry |= (dest << 56);
    entry |= RTL8139_INT_VECTOR;
    entry |= (1 << 13); // pci devices normally expect polarity low
    entry |= (1 << 15); // and level triggered
    CpuIoApicSetRedirEntry(Gsi, entry);
    CpuRegisterHandler(RTL8139_INT_VECTOR, Rtl8139InterruptHandler);
    NetRegisterNic(Nic);
    return KSUCCESS;
}