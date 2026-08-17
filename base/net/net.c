#include <net/net.h>
#include <kedriver.h>
#include <stddef.h>
#include <mm/heap.h>
#include <memory.h>
#include <util/util.h>
#include <printfwrapper.h>
// list of netinterface structs
static NetInterface* gNetInterfaceHead = NULL;

NetInterface* NetGetLinkedList() {
    return gNetInterfaceHead;
}

KSTATUS NetRegisterNic(NetInterface* Nic) {
    Nic->Next = gNetInterfaceHead;
    gNetInterfaceHead = Nic; 
    if (Nic->MacAddress && !KernelGetInformation()->net.Mac) {
        printf("net: using nic mac as kernel default MAC.\r\n");
        memcpy((void*)KernelGetInformation()->net.Mac, Nic->MacAddress, 6);
    }   
    return KSUCCESS;
}

KE_EXPORT_SYMBOL(NetRegisterNic);

KSTATUS NetWriteRaw(NetInterface* Nic, void* Buffer, uint16_t Length) {
    KeIoRequest Irp;
    Irp.Major = IO_WRITE;
    Irp.Buffer = Buffer;
    Irp.Length = Length;
    Irp.ReadBytes = 0;
    KSTATUS r = KeIoDispatch(Nic->Device, &Irp);
    return r;
}

KSTATUS NetReadRaw(NetInterface* Nic, void* Buffer, uint16_t Length, uint16_t* BytesReadOut) {
    KeIoRequest Irp;
    Irp.Major = IO_READ;
    Irp.Buffer = Buffer;
    Irp.Length = Length;
    Irp.ReadBytes = 0;
    KSTATUS r = KeIoDispatch(Nic->Device, &Irp);
    if (BytesReadOut) *BytesReadOut = (uint16_t)Irp.ReadBytes;
    return r;
}

KSTATUS NetArpReply(NetInterface* Nic, uint8_t* ToMacAddress, uint8_t* FromMacAddress, uint8_t* ToIpAddress, uint8_t* FromIpAddress) {
    void* Buffer = MmAllocate(sizeof(NetEthFrameHdr) + sizeof(NetArpHdr));
    NetEthFrameHdr* EthFrame = (NetEthFrameHdr*)Buffer;
    memcpy((void*)EthFrame->DestMac, ToMacAddress, 6);
    memcpy((void*)EthFrame->SrcMac, FromMacAddress, 6);
    EthFrame->EtherType = UtilSwapEnd16(0x0806);
    NetArpHdr* ArpHdr = (NetArpHdr*)((uint64_t)EthFrame + sizeof(NetEthFrameHdr));
    ArpHdr->Opcode = UtilSwapEnd16(2);
    ArpHdr->HType = UtilSwapEnd16(1);
    ArpHdr->PType = UtilSwapEnd16(0x0800);
    ArpHdr->HwAddrLen = 6;
    ArpHdr->PrAddrLen = 4;
    memcpy((void*)ArpHdr->SrcHw, FromMacAddress, 6);
    memcpy((void*)ArpHdr->DestHw, ToMacAddress, 6);
    memcpy((void*)ArpHdr->SrcPr, FromIpAddress, 6);
    memcpy((void*)ArpHdr->DestPr, ToIpAddress, 6);
    KSTATUS r = NetWriteRaw(Nic, Buffer, sizeof(NetEthFrameHdr) + sizeof(NetArpHdr));
    MmFree(Buffer);
    return r;
}

KSTATUS NetArpRequest(NetInterface* Nic, uint8_t* ToMacAddress, uint8_t* FromMacAddress, uint8_t* FromIpAddress) {
    void* Buffer = MmAllocate(sizeof(NetEthFrameHdr) + sizeof(NetArpHdr));
    NetEthFrameHdr* EthFrame = (NetEthFrameHdr*)Buffer;
    memcpy((void*)EthFrame->DestMac, ToMacAddress, 6);
    memcpy((void*)EthFrame->SrcMac, FromMacAddress, 6);
    EthFrame->EtherType = UtilSwapEnd16(0x0806);
    NetArpHdr* ArpHdr = (NetArpHdr*)((uint64_t)EthFrame + sizeof(NetEthFrameHdr));
    ArpHdr->Opcode = UtilSwapEnd16(1);
    ArpHdr->HType = UtilSwapEnd16(1);
    ArpHdr->PType = UtilSwapEnd16(0x0800);
    ArpHdr->HwAddrLen = 6;
    ArpHdr->PrAddrLen = 4;
    memcpy((void*)ArpHdr->SrcHw, FromMacAddress, 6);
    memcpy((void*)ArpHdr->DestHw, ToMacAddress, 6);
    memcpy((void*)ArpHdr->SrcPr, FromIpAddress, 6);
    memset((void*)ArpHdr->DestPr, 0, 6);
    KSTATUS r = NetWriteRaw(Nic, Buffer, sizeof(NetEthFrameHdr) + sizeof(NetArpHdr));
    MmFree(Buffer);
    return r;
}

// should be called by network driver on every packet
// so we can see if its an ARP request
KSTATUS NetHandlePacket(NetInterface* Nic, void* Buffer, uint16_t Length) {
    if (!Buffer || Length == 0) return KINVALID;
    if (!Nic) return KINVALID;
    NetEthFrameHdr* EthFrame = (NetEthFrameHdr*)Buffer;
    printf("net: Packet from [");
    UtilPrintMacAddr(EthFrame->SrcMac);
    printf("] to [");
    UtilPrintMacAddr(EthFrame->DestMac);
    printf("]\r\n");
    if (UtilSwapEnd16(EthFrame->EtherType) == 0x0806) {
        // is arp packet
        NetArpHdr* ArpHdr = (NetArpHdr*)((uint64_t)EthFrame + sizeof(NetEthFrameHdr));
        printf("net: Packet is ARP packet.\r\n");
        if (UtilSwapEnd16(ArpHdr->Opcode) == 1) {
            printf("net: Opcode type is Request.\r\n");
        } else if (UtilSwapEnd16(ArpHdr->Opcode) == 2) {
            printf("net: Opcode type is Reply.\r\n");
        }
        printf("net: ARP Packet from [%d.%d.%d.%d] to [%d.%d.%d.%d]\r\n", ArpHdr->SrcPr[0], ArpHdr->SrcPr[1], ArpHdr->SrcPr[2], ArpHdr->SrcPr[3], ArpHdr->DestPr[0], ArpHdr->DestPr[1], ArpHdr->DestPr[2], ArpHdr->DestPr[3]);
        // send a reply
        if (KernelGetInformation()->net.Ip[0] == 0) {
            printf("net: warn: our ip hasn't been initalized (Ip[0] = 0)\r\n");
            return KUNSUPPORTED;
        }
        int IsIp = memcmp((void*)ArpHdr->DestPr, (void*)KernelGetInformation()->net.Ip, 4);
        if (UtilSwapEnd16(ArpHdr->Opcode) == 1 && (IsIp == 0)) {
            KSTATUS r2 = NetArpReply(Nic, ArpHdr->SrcHw, Nic->MacAddress, ArpHdr->SrcPr, ArpHdr->DestPr);
            if (r2 != KSUCCESS) {
                printf("net: warn: replying to arp request failed!\r\n");
            }
        }
        return KSUCCESS;
    }
    // note we dont free the buffer mainly cuz the proper NetRead call might still be using it
    // so we should let that free it
    return KUNSUPPORTED;
}
KE_EXPORT_SYMBOL(NetHandlePacket);