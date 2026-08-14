#include <kernel.h>
#include <kedriver.h>
#include <net/net.h>
#include <stddef.h>
#include <arch/x86_64/pci/pci.h>
#include <arch/x86_64/ports.h>
#include <memory.h>
#include <mm/heap.h>
#include <mm/pmm.h>
#include <arch/x86_64/cpu/ioapic.h>
#include <arch/x86_64/cpu/lapic.h>
#include <arch/x86_64/cpu/idt.h>
#include <util/spinlock.h>
#include <util/util.h>

// bs taken straight from linux kernel
#define NE2K_TX_START_PAGE 0x40
#define NE2K_TX_STOP_PAGE 0x80
#define NE2K_INT_VECTOR 0x30

#define NE2K_ISR_PRX  (1 << 0)
#define NE2K_ISR_PTX  (1 << 1)
#define NE2K_ISR_RXE  (1 << 2)
#define NE2K_ISR_TXE  (1 << 3)
#define NE2K_ISR_OVW  (1 << 4)
#define NE2K_ISR_CNT  (1 << 5)
#define NE2K_ISR_RDC  (1 << 6)
#define NE2K_ISR_RST  (1 << 7)

#define NE2K_IMR_PRXE (1 << 0)
#define NE2K_IMR_PTXE (1 << 1)
#define NE2K_IMR_RXEE (1 << 2) 
#define NE2K_IMR_TXEE (1 << 3)
#define NE2K_IMR_OVWE (1 << 4)
#define NE2K_IMR_CNTE (1 << 5)

// ne2k specific packet
typedef struct _Ne2kPacket {
    uint8_t* Data;
    uint16_t Length;
    uint8_t Page;
    struct _Ne2kPacket* Next;
} Ne2kPacket;

typedef struct {
    Ne2kPacket* Head;
    Ne2kPacket* Tail;
} Ne2kPacketQueue;

NetInterface* stupid;
Ne2kPacketQueue* PckQueue;
Spinlock PckLck;
int TxInProgress;

void Ne2kQueueAdd(Ne2kPacketQueue* Queue, uint8_t* Data, uint16_t Length) {
    Ne2kPacket* Packet = (Ne2kPacket*)MmAllocate(sizeof(Ne2kPacket));
    Packet->Data = (uint8_t*)MmAllocate(Length);
    memcpy(Packet->Data, Data, Length);
    Packet->Length = Length;
    Packet->Next = NULL;
    if (Queue->Tail) {
        Queue->Tail->Next = Packet;
    } else {
        Queue->Head = Packet;
    }
    Queue->Tail = Packet;
}

Ne2kPacket* Ne2kQueueGet(Ne2kPacketQueue* Queue) {
    if (!Queue->Head) return NULL;
    Ne2kPacket* Packet = Queue->Head;
    Queue->Head = Packet->Next;
    if (!Queue->Head) Queue->Tail = NULL;
    return Packet;
}

int Ne2kQueueIsEmpty(Ne2kPacketQueue* Queue) {
    return Queue->Head == NULL;
}

KSTATUS Ne2kWriteReg(NetInterface* Nic, uint8_t Page, uint8_t Reg, uint8_t Value) {
    outb((uint16_t)Nic->IoBase, (Page * 0x20));
    outb((uint16_t)Nic->IoBase + Reg, Value);
    return KSUCCESS;
}

// Buffer expected to be a physical one
KSTATUS Ne2kSendPacketRaw(NetInterface* Nic, uint16_t Size, uint8_t Page, void* Buffer) {
    outb((uint16_t)Nic->IoBase, 0x22);
    outb((uint16_t)Nic->IoBase + 0x04, Page);
    outb((uint16_t)Nic->IoBase + 0x0A, Size & 0x00FF);
    outb((uint16_t)Nic->IoBase + 0x0B, (Size >> 8) & 0x00FF);
    uint8_t isr = inb((uint16_t)Nic->IoBase + 0x07);
    isr |= (1 << 6);
    outb((uint16_t)Nic->IoBase + 0x07, isr);
    uint16_t Paddr = (uint16_t)(Page * 256);
    outb((uint16_t)Nic->IoBase + 0x08, Paddr & 0x00FF);
    outb((uint16_t)Nic->IoBase + 0x09, (Paddr >> 8) & 0x00FF);
    outb((uint16_t)Nic->IoBase, 0x12);
    uint8_t* U8Buf = (uint8_t*)P2V(Buffer);
    for (uint16_t i = 0; i < Size; i++) {
        outb((uint16_t)Nic->IoBase + 0x10, (uint8_t)U8Buf[i]);
    }
    while ((inb((uint16_t)Nic->IoBase + 0x07) & (1 << 6)) == 0);
    outb((uint16_t)Nic->IoBase + 0x05, Size & 0x00FF);
    outb((uint16_t)Nic->IoBase + 0x06, (Size >> 8) & 0x00FF);

    outb((uint16_t)Nic->IoBase, 0x24); 

    return KSUCCESS;
}

void Ne2kSendPacket(uint8_t* Data, uint16_t Length) {
    uint64_t r = SpnLckAcquireRfl(&PckLck);
    if (TxInProgress) {
        Ne2kQueueAdd(PckQueue, Data, Length);
    } else {
        TxInProgress = 1;
        void* Phys = PmmAllocatePages(UTIL_DIV_RUP(Length, MMU_PAGE_SIZE));
        uint8_t* VBuf = (uint8_t*)P2V(Phys);
        memcpy((void*)VBuf, Data, Length);
        Ne2kSendPacketRaw(stupid, Length, NE2K_TX_START_PAGE, Phys);
        PmmFreePages(Phys, UTIL_DIV_RUP(Length, MMU_PAGE_SIZE));
    }
    SpnLckReleaseRfl(&PckLck, r);
}
void Ne2kIrqHandler(CpuInterruptArgs* r) {
    uint8_t isr = inb((uint16_t)stupid->IoBase + 0x07);
    if (isr & NE2K_ISR_PRX) {
        // todo
        KeDrvWrite("ne2k: irq: received packet.\r\n");
    }
    if (isr & NE2K_ISR_RXE) {
        // todo
        KeDrvWrite("ne2k: irq: transmit packet finished.\r\n");
    }
    KeDrvWrite("ne2k: irq: clearing iobase bits.\r\n");
    outb((uint16_t)stupid->IoBase + 0x07, isr); // clear bits otherwise bad stuff might happen
    // see whos next in queue and handle their packet too
    uint64_t rfl = SpnLckAcquireRfl(&PckLck); // not a good idea to lock inside an irq
    TxInProgress = 0;
    if (!Ne2kQueueIsEmpty(PckQueue)) {
        Ne2kPacket* Next = Ne2kQueueGet(PckQueue);
        TxInProgress = 1;
        void* Phys = PmmAllocatePages(UTIL_DIV_RUP(Next->Length, MMU_PAGE_SIZE));
        uint8_t* VBuf = (uint8_t*)P2V(Phys);
        memcpy((void*)VBuf, Next->Data, Next->Length);
        Ne2kSendPacketRaw(stupid, Next->Length, NE2K_TX_START_PAGE, Phys);
        PmmFreePages(Phys, UTIL_DIV_RUP(Next->Length, MMU_PAGE_SIZE));
    }
    SpnLckReleaseRfl(&PckLck, rfl);
    CpuLapicEoi();
}

KSTATUS DriverEntry(KeDriverObj* Self) {
    memcpy(Self->Name, "ne2k-nic", 9);
    KeDeviceObj* device = MmAllocate(sizeof(KeDeviceObj));
    memcpy(device->Name, "ne2k", 5);
    device->Owner = Self;
    device->Device = NULL;
    device->Next = NULL;

    // look for device in pci list
    KePciDeviceHdr* PciDev = PciGetLinkedList();
    while (PciDev != NULL) {
        if (PciDev->Header->VendorID == 0x10ec && PciDev->Header->DeviceID == 0x8029) {;
            break; // found
        }
        PciDev = PciDev->Next;
    }
    if (PciDev == NULL) {
        // no ne2k in the system
        MmFree(device);
        return KUNSUPPORTED;
    }
    KeRegisterDevice(device);
    NetInterface* Nic = MmAllocate(sizeof(NetInterface));
    memcpy(Nic->Name, "ne2k_eth0", 10);
    // based off https://wiki.osdev.org/Ne2000
    Nic->IoBase = ((PciDeviceHeaderTy0*)PciDev->Header)->BAR0 & ~0x3;
    KeDrvWriteFmt("ne2k: pci int line 0x%lx pci int pin 0x%lx\r\n", ((PciDeviceHeaderTy0*)PciDev->Header)->InterruptLine, ((PciDeviceHeaderTy0*)PciDev->Header)->InterruptPin);
    Nic->Next = NULL;
    Nic->Device = device;
    uint32_t Cmd = PciReadDword(PciDev->EcamBase, PciDev->Bus, PciDev->Dev, PciDev->Func, 0x04);
    Cmd |= (1 << 0);
    Cmd |= (1 << 2);
    Cmd &= ~(1 << 10); // if for some reason interrupt disable bit is 1 clear it
    PciWriteDword(PciDev->EcamBase, PciDev->Bus, PciDev->Dev, PciDev->Func, 0x04, Cmd);
    // write reset
    outb((uint16_t)Nic->IoBase + 0x1F, inb((uint16_t)Nic->IoBase + 0x1F));
    // wait for reset
    while ((inb((uint16_t)Nic->IoBase + 0x07) & 0x80) == 0);
    // prob should use Ne2kWriteReg but i wrote this before i added that
    outb((uint16_t)Nic->IoBase + 0x07, 0xFF);
    uint8_t Rom[32];
    outb((uint16_t)Nic->IoBase, (1 << 5) | 1);
    outb((uint16_t)Nic->IoBase + 0x0E, 0x49);
    outb((uint16_t)Nic->IoBase + 0x0A, 0);
    outb((uint16_t)Nic->IoBase + 0x0B, 0);
    outb((uint16_t)Nic->IoBase + 0x0F, 0);
    outb((uint16_t)Nic->IoBase + 0x07, 0xFF);
    outb((uint16_t)Nic->IoBase + 0x0C, 0x20);
    outb((uint16_t)Nic->IoBase + 0x0D, 0x02);
    outb((uint16_t)Nic->IoBase + 0x0A, 32);
    outb((uint16_t)Nic->IoBase + 0x0B, 0);
    outb((uint16_t)Nic->IoBase + 0x08, 0);
    outb((uint16_t)Nic->IoBase + 0x09, 0);
    outb((uint16_t)Nic->IoBase + 0x0A, 0);
    // read ROM
    for (int i = 0; i < 32; i++) {
        Rom[i] = inb(Nic->IoBase + 0x10);
    }
    memcpy((void*)Nic->MacAddress, (const void*)Rom, 6);
    // listen for packets
    for (int i = 0; i < 6; i++) Ne2kWriteReg(Nic, 1, 0x01+i, Rom[i]);

    uint64_t entry = 0;
    uint64_t dest = CpuLapicGetId();
    entry |= (dest << 56);

    uint8_t vector = NE2K_INT_VECTOR;
    entry |= vector;

    entry |= (1ULL << 13);
    entry |= (1ULL << 15);

    CpuIoApicSetRedirEntry(((PciDeviceHeaderTy0*)PciDev->Header)->InterruptLine, entry); 
    stupid = Nic;
    TxInProgress = 0;
    CpuRegisterHandler(NE2K_INT_VECTOR, Ne2kIrqHandler);
    // also ne2k should send interrupts
    outb((uint16_t)Nic->IoBase + 0x0F, NE2K_IMR_PRXE | NE2K_IMR_PTXE | NE2K_IMR_RXEE | NE2K_IMR_TXEE | NE2K_IMR_OVWE);
    outb((uint16_t)Nic->IoBase + 0x0C, (1 << 2) | (1 << 4));
    outb((uint16_t)Nic->IoBase, (1 << 1)); // actually start the card
    NetRegisterNic(Nic);
    KeDrvWrite("ne2k: registered irq handler and nic inside kernel nic list.\r\n");
    return KSUCCESS;
}