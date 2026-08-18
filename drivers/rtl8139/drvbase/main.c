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
#include <sched/sched.h>
#include <sched/process.h>

#define RTL8139_RXBUF_SZ 8192+16+1500
#define RTL8139_INT_VECTOR 0x30

typedef struct Rtl8139Packet {
    void* Buffer;
    uint16_t Length;
    struct Rtl8139Packet* Next;
} Rtl8139Packet;

typedef struct {
    Rtl8139Packet* Head;
    Rtl8139Packet* Tail;
} Rtl8139PacketQueue;

typedef struct {
    physaddr TxSavedAddrs[4];
    uint32_t TxSavedSizes[4];
    uint16_t RxReadOffset;
    Rtl8139PacketQueue RxQueue; // this is what read would read from and isr would push data onto there
} Rtl8139DriverSt;

static NetInterface* gNic; // shouldnt break even with more than 1 rtl8139 cuz we only initalize the first one we find
void Rtl8139InterruptHandler(CpuInterruptArgs* r) {
    uint16_t Status = inw(gNic->IoBase + 0x3E);
    outw(gNic->IoBase + 0x3E, 0x05);
    // received packet
    if (Status & 0x1) {
        while (!(inb(gNic->IoBase + 0x37) & 0x01))  {
            Rtl8139DriverSt* DrvSt = (Rtl8139DriverSt*)gNic->DriverState;
            uint16_t Capr = inw(gNic->IoBase + 0x38);
            uint16_t Offset = (Capr + 16) % RTL8139_RXBUF_SZ;
            physaddr RxAddr = gNic->RxBuffer + DrvSt->RxReadOffset;
            void* VRxAddr = (void*)P2V(RxAddr);
            uint16_t PckStatus = (uint16_t)(*(uint32_t*)VRxAddr & 0xFFFF);
            uint16_t PckLength = (uint16_t)(*(uint32_t*)VRxAddr >> 16) & 0xFFFF;
            if (!PckStatus || PckStatus == 0xe1e3) {
                goto _recv_error;
            };
            uint8_t *FrameData = (uint8_t*)(VRxAddr + 4);
            DrvSt->RxReadOffset = (DrvSt->RxReadOffset + PckLength + 4 + 3) & ~3;
            DrvSt->RxReadOffset %= RTL8139_RXBUF_SZ;
            outw(gNic->IoBase + 0x38, DrvSt->RxReadOffset - 16);
            uint8_t CrAfter = inb(gNic->IoBase + 0x37);
            Rtl8139Packet* Packet = MmAllocate(sizeof(Rtl8139Packet));
            Packet->Buffer = MmAllocate(PckLength - 4); // apparently the card can js start writing new data if we use the direct framedata ptr
            memcpy(Packet->Buffer, FrameData, PckLength - 4);
            Packet->Length = PckLength;
            Packet->Next = NULL;
            if (DrvSt->RxQueue.Tail) {
                DrvSt->RxQueue.Tail->Next = Packet;
            } else {
                DrvSt->RxQueue.Head = Packet;
            }
            DrvSt->RxQueue.Tail = Packet;
            // KeDrvWriteFmt("rtl8139: adding to queue @ ring offset 0x%x\r\n", DrvSt->RxReadOffset);
            // also call kernel callback
            KSTATUS r = NetHandlePacket(gNic, Packet->Buffer, Packet->Length);
            Capr = inw(gNic->IoBase + 0x38); // reread
            uint16_t Cbr = inw(gNic->IoBase + 0x3A);
            if (DrvSt->RxReadOffset == Cbr) {
                break; // empty and BUFE is lying
            }
        }

    }
    // transmit packet success
    if (Status & (1 << 2)) {
        Rtl8139DriverSt* DrvSt = (Rtl8139DriverSt*)gNic->DriverState;
        for (int i = 0; i < 4; i++) {
            uint32_t PortStatus = inl(gNic->IoBase + 0x10 + (i * 4));
            if ((PortStatus & (1 << 15)) && DrvSt->TxSavedAddrs[i]) {
                PmmFreePages((void*)DrvSt->TxSavedAddrs[i], DrvSt->TxSavedSizes[i]);
                DrvSt->TxSavedAddrs[i] = 0;
                DrvSt->TxSavedSizes[i] = 0;
            }
        }
    }
    if (Status & (1 << 1)) {
        _recv_error:
        KeDrvWrite("rtl8139: error while receiving packet.\r\n");
    }
    if (Status & (1 << 3)) {
        KeDrvWrite("rtl8139: error while transmitting packet.\r\n");
    }
    ThreadCtrlBlk* SuspendedThr = ThreadPopHead(&gNic->RxWaitListHead, &gNic->RxWaitListTail);
    if (SuspendedThr != NULL) {
        ThreadWake(SuspendedThr);
    }
    CpuLapicEoi();
}


KSTATUS Rtl8139Transmit(NetInterface* Nic, void* Data, uint16_t Length) {
    if (Length < 60) Length = 60; // padding cuz eth min frame
    if (Length > 1792) return KINVALID; // oversized
    void* TxBufferPhys = PmmAllocatePages(UTIL_DIV_RUP(Length, MMU_PAGE_SIZE));
    memset((void*)P2V(TxBufferPhys), 0, UTIL_DIV_RUP(Length, MMU_PAGE_SIZE));
    memcpy((void*)P2V(TxBufferPhys), Data, Length);
    uint32_t TsadReg = 0x20 + (Nic->NextTxDesc * 4);
    uint32_t TsdReg  = 0x10 + (Nic->NextTxDesc * 4);
    outl(Nic->IoBase + TsadReg, (uint32_t)TxBufferPhys);
    outl(Nic->IoBase + TsdReg, Length);
    Rtl8139DriverSt* DrvSt =  (Rtl8139DriverSt*)Nic->DriverState;
    DrvSt->TxSavedAddrs[Nic->NextTxDesc] = (physaddr)TxBufferPhys;
    DrvSt->TxSavedSizes[Nic->NextTxDesc] = UTIL_DIV_RUP(Length, MMU_PAGE_SIZE);
    Nic->NextTxDesc = (Nic->NextTxDesc + 1) % 4;
    return KSUCCESS;
}

int Rtl8139RmQueue(Rtl8139DriverSt* DrvSt, Rtl8139Packet** PckOut) {
    if (!DrvSt->RxQueue.Head && !DrvSt->RxQueue.Tail) {
        return 0;
    }
    Rtl8139Packet* R = DrvSt->RxQueue.Head;
    DrvSt->RxQueue.Head = DrvSt->RxQueue.Head->Next;
    if (!DrvSt->RxQueue.Head) {
        DrvSt->RxQueue.Tail = NULL;
    }
    R->Next = NULL;
    *PckOut = R;
    // KeDrvWriteFmt("rtl8139: removing packet{base=0x%lx, bufferaddr=0x%lx, length=%d} from queue\r\n", R, R->Buffer, R->Length);
    return 1;
}

// expose to rest of os
KSTATUS Rtl8139Write(KeDeviceObj* dev, KeIoRequest* irp) {
    if (!dev) return KINVALID;
    if (!irp) return KINVALID;
    if (irp->Major != IO_WRITE) return KINVALID;
    if (!irp->Buffer || irp->Length == 0) return KINVALID;
    KSTATUS r = Rtl8139Transmit(gNic, irp->Buffer, irp->Length);
    if (r != KSUCCESS) {
        irp->ReadBytes = (uint64_t)-1;
        return r;
    }
    irp->ReadBytes = irp->Length;
    return r;
}

KSTATUS Rtl8139Read(KeDeviceObj* dev, KeIoRequest* irp) {
    if (!dev) return KINVALID;
    if (!irp) return KINVALID;
    if (irp->Major != IO_READ) return KINVALID;
    if (!irp->Buffer || irp->Length == 0) return KINVALID;

    uint8_t* UserBuf = (uint8_t*)irp->Buffer;
    Rtl8139DriverSt* DrvSt =  (Rtl8139DriverSt*)gNic->DriverState;
    Rtl8139Packet* Pck;
    while (!Rtl8139RmQueue(DrvSt, &Pck)) {
        ThreadCtrlBlk* cthr = ThrGetCurrent();
        cthr->state = SCHED_THREAD_SUSPENDED;
        ThreadPushTail(&gNic->RxWaitListHead, &gNic->RxWaitListTail, cthr);
        SchedYield();
    }
    uint64_t CopyLen = (Pck->Length < irp->Length) ? Pck->Length : irp->Length;
    memcpy(UserBuf, Pck->Buffer, CopyLen);
    MmFree(Pck->Buffer);
    MmFree(Pck);
    irp->ReadBytes = CopyLen;
    return KSUCCESS;
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
    device->Dispatch[IO_WRITE] = Rtl8139Write;
    device->Dispatch[IO_READ] = Rtl8139Read;
    KeRegisterDevice(device);
    NetInterface* Nic = MmAllocate(sizeof(NetInterface));
    memcpy(Nic->Name, "rtl8139_eth0", 13);
    Nic->NextTxDesc = 0;
    Nic->Device = device;
    Nic->IoBase = ((PciDeviceHeaderTy0*)PciDev->Header)->BAR0 & ~0x3;
    Nic->DriverState = MmAllocate(sizeof(Rtl8139DriverSt));
    memset((void*)Nic->DriverState, 0, sizeof(Rtl8139DriverSt));
    Rtl8139DriverSt* s = Nic->DriverState;
    s->RxQueue.Head = NULL;
    s->RxQueue.Tail = NULL;
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
    gNic = Nic;
    NetRegisterNic(Nic);
    return KSUCCESS;
}