#include <net/net.h>
#include <kedriver.h>
#include <stddef.h>
#include <mm/heap.h>
#include <memory.h>
#include <util/util.h>
#include <printfwrapper.h>
// list of netinterface structs
static NetInterface* gNetInterfaceHead = NULL;
static NetCallback gNetCallbacks[256];

NetInterface* NetGetLinkedList() {
    return gNetInterfaceHead;
}

KSTATUS NetRegisterCallback(uint16_t At, NetCallback* callback) {
    if (At >= 256) return KINVALID;
    gNetCallbacks[At] = *callback;
    return KSUCCESS;
} 

KSTATUS NetDeregisterCalback(uint16_t At) {
    if (At >= 256) return KINVALID;
    gNetCallbacks[At].CallBack = 0;
    gNetCallbacks[At].Type = (NetCallbackType)-1;
    return KSUCCESS;
}


KSTATUS NetRegisterNic(NetInterface* Nic) {
    Nic->Next = gNetInterfaceHead;
    gNetInterfaceHead = Nic; 
    if (Nic->MacAddress) {
        printf("net: overwriting current MAC with new NIC.\r\n");
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

KSTATUS NetArpRequest(NetInterface* Nic, uint8_t* ToMacAddress, uint8_t* FromMacAddress, uint8_t* ToIpAddress, uint8_t* FromIpAddress) {
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
    memcpy((void*)ArpHdr->DestPr, ToIpAddress, 6);
    KSTATUS r = NetWriteRaw(Nic, Buffer, sizeof(NetEthFrameHdr) + sizeof(NetArpHdr));
    MmFree(Buffer);
    return r;
}
// https://datatracker.ietf.org/doc/html/rfc1624
static uint16_t NetChecksum(void* Buffer, uint64_t Size) {
    const uint16_t *ptr = (const uint16_t *)Buffer;
    uint32_t sum = 0;

    while (Size > 1) {
        sum += *ptr++;
        Size -= 2;
    }
    if (Size > 0) {
        uint16_t odd_byte = 0;
        *(uint8_t *)&odd_byte = *(const uint8_t *)ptr;
        sum += odd_byte;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    // do bitwise not for 1s complement
    return (uint16_t)(~sum);
}

KSTATUS NetIcmpEchoReply(NetInterface* Nic, uint8_t* ToIpAddress, NetIcmpEchoHdr* OgEchoHdr, uint16_t OgIpv4HdrLength, void* OgPayload) {
    NetArpEntry* Entry = NetArpTableResolve(&KernelGetInformation()->net.ArpHead, ToIpAddress);
    if (!Entry) return KINVALID;
    uint16_t PayloadLen = OgIpv4HdrLength - sizeof(NetIpv4Hdr) - sizeof(NetIcmpHdr) - sizeof(NetIcmpEchoHdr);
    uint32_t IcmpPacketLen = sizeof(NetIcmpHdr) + sizeof(NetIcmpEchoHdr) + PayloadLen;
    uint32_t FrameLen = sizeof(NetEthFrameHdr) + sizeof(NetIpv4Hdr) + IcmpPacketLen;
    void* Buffer = MmAllocate(FrameLen);
    memset((void*)Buffer, 0, FrameLen);
    NetEthFrameHdr* EthFrame = (NetEthFrameHdr*)Buffer;
    memcpy((void*)EthFrame->DestMac, Entry->Mac, 6);
    memcpy((void*)EthFrame->SrcMac, KernelGetInformation()->net.Mac, 6);
    EthFrame->EtherType = UtilSwapEnd16(0x0800);
    NetIpv4Hdr* Ipv4Hdr = (NetIpv4Hdr*)((uint64_t)EthFrame + sizeof(NetEthFrameHdr));
    Ipv4Hdr->Version = 4;
    Ipv4Hdr->InternetHdrLength = 5;
    Ipv4Hdr->Dscp = 0;
    Ipv4Hdr->Ecn = 0;
    Ipv4Hdr->Length = UtilSwapEnd16(sizeof(NetIpv4Hdr) + sizeof(NetIcmpHdr) + sizeof(NetIcmpEchoHdr) + PayloadLen);
    Ipv4Hdr->Id = 0;
    Ipv4Hdr->Flags = 0;
    Ipv4Hdr->FragmentOff = 0;
    Ipv4Hdr->Ttl = 64;
    Ipv4Hdr->Protocol = NET_IPV4_PROTOCOL_ICMP;

    memcpy((void*)Ipv4Hdr->Sender, KernelGetInformation()->net.Ip, 4);
    memcpy((void*)Ipv4Hdr->Destination, ToIpAddress, 4);
    // i aint tryna do shi with checksums bro
    Ipv4Hdr->HdrChecksum = 0;
    Ipv4Hdr->HdrChecksum = (NetChecksum((void*)Ipv4Hdr, Ipv4Hdr->InternetHdrLength * 4));

    NetIcmpHdr* IcmpHdr = (NetIcmpHdr*)((uint64_t)EthFrame + sizeof(NetEthFrameHdr) + sizeof(NetIpv4Hdr));
    IcmpHdr->Type = 0;
    IcmpHdr->Code = 0;
    IcmpHdr->Checksum = 0;
    NetIcmpEchoHdr* EchoHdr = (NetIcmpEchoHdr*)((uint64_t)IcmpHdr + sizeof(NetIcmpHdr));
    EchoHdr->Id = OgEchoHdr->Id;
    EchoHdr->Sequence = OgEchoHdr->Sequence;
    void* OutPayload = (void*)((uint64_t)EchoHdr + sizeof(NetIcmpEchoHdr));
    memcpy((void*)OutPayload, OgPayload, PayloadLen);
    IcmpHdr->Checksum = (NetChecksum((void*)IcmpHdr, sizeof(NetIcmpHdr) + sizeof(NetIcmpEchoHdr) + PayloadLen));
    KSTATUS r = NetWriteRaw(Nic, Buffer, FrameLen);
    MmFree(Buffer);
    return r;
}


KSTATUS NetIcmpEchoRequest(NetInterface* Nic, uint8_t* ToIpAddress, uint16_t Sequence) {
    NetArpEntry* Entry = NetArpTableResolve(&KernelGetInformation()->net.ArpHead, ToIpAddress);
    if (!Entry) return KINVALID;
    uint32_t IcmpPacketLen = sizeof(NetIcmpHdr) + sizeof(NetIcmpEchoHdr) + 64;
    uint32_t FrameLen = sizeof(NetEthFrameHdr) + sizeof(NetIpv4Hdr) + IcmpPacketLen;
    void* Buffer = MmAllocate(FrameLen);
    memset((void*)Buffer, 0, FrameLen);
    NetEthFrameHdr* EthFrame = (NetEthFrameHdr*)Buffer;
    memcpy((void*)EthFrame->DestMac, Entry->Mac, 6);
    memcpy((void*)EthFrame->SrcMac, KernelGetInformation()->net.Mac, 6);
    EthFrame->EtherType = UtilSwapEnd16(0x0800);
    NetIpv4Hdr* Ipv4Hdr = (NetIpv4Hdr*)((uint64_t)EthFrame + sizeof(NetEthFrameHdr));
    Ipv4Hdr->Version = 4;
    Ipv4Hdr->InternetHdrLength = 5;
    Ipv4Hdr->Dscp = 0;
    Ipv4Hdr->Ecn = 0;
    Ipv4Hdr->Length = UtilSwapEnd16(sizeof(NetIpv4Hdr) + IcmpPacketLen);
    Ipv4Hdr->Id = 0;
    Ipv4Hdr->Flags = 0;
    Ipv4Hdr->FragmentOff = 0;
    Ipv4Hdr->Ttl = 64;
    Ipv4Hdr->Protocol = NET_IPV4_PROTOCOL_ICMP;

    memcpy((void*)Ipv4Hdr->Sender, KernelGetInformation()->net.Ip, 4);
    memcpy((void*)Ipv4Hdr->Destination, ToIpAddress, 4);
    // i aint tryna do shi with checksums bro
    Ipv4Hdr->HdrChecksum = 0;
    Ipv4Hdr->HdrChecksum = (NetChecksum((void*)Ipv4Hdr, Ipv4Hdr->InternetHdrLength * 4));

    NetIcmpHdr* IcmpHdr = (NetIcmpHdr*)((uint64_t)EthFrame + sizeof(NetEthFrameHdr) + sizeof(NetIpv4Hdr));
    IcmpHdr->Type = 8;
    IcmpHdr->Code = 0;
    IcmpHdr->Checksum = 0;
    NetIcmpEchoHdr* EchoHdr = (NetIcmpEchoHdr*)((uint64_t)IcmpHdr + sizeof(NetIcmpHdr));
    EchoHdr->Id = 0;
    EchoHdr->Sequence = UtilSwapEnd16(Sequence);
    IcmpHdr->Checksum = (NetChecksum((void*)IcmpHdr, sizeof(NetIcmpHdr) + sizeof(NetIcmpEchoHdr) + 64));
    KSTATUS r = NetWriteRaw(Nic, Buffer, FrameLen);
    MmFree(Buffer);
    return r;
}


KSTATUS NetIpv4Handle(NetEthFrameHdr* EthFrame, NetIpv4Hdr* Ipv4) {
    switch (Ipv4->Protocol) {
        case NET_IPV4_PROTOCOL_ICMP: {
            NetIcmpHdr* IcmpHdr = (NetIcmpHdr*)((uint64_t)EthFrame + sizeof(NetEthFrameHdr) + sizeof(NetIpv4Hdr));
            if (IcmpHdr->Type == 0) { // echo reply
                NetIcmpEchoHdr* EchoHdr = (NetIcmpEchoHdr*)((uint64_t)IcmpHdr + sizeof(NetIcmpHdr));
                for (int i = 0; i < 256; i++) {
                    if (gNetCallbacks[i].CallBack && gNetCallbacks[i].Type == NET_CALLBACK_ICMP) {
                        gNetCallbacks[i].CallBack(EthFrame, Ipv4, EchoHdr, IcmpHdr);
                    }
                }
                return KSUCCESS;
            }

            if (IcmpHdr->Type == 8) { // echo request
                NetIcmpEchoHdr* EchoHdr = (NetIcmpEchoHdr*)((uint64_t)IcmpHdr + sizeof(NetIcmpHdr));
                KSTATUS r = NetIcmpEchoReply(NetGetLinkedList(), Ipv4->Sender, EchoHdr, UtilSwapEnd16(Ipv4->Length), (void*)((uint64_t)EchoHdr + sizeof(NetIcmpEchoHdr)));
                return r;
            }
            break;
        }
        default: {
            printf("net: not handling ipv4 packet because no handler for it.\r\n");
            return KUNSUPPORTED;
        }
    }
    return KSUCCESS; 
}
KSTATUS NetArpTableAdd(NetArpEntry** Table, uint8_t* Mac, uint8_t* Ip) {
    NetArpEntry* Entry = MmAllocate(sizeof(NetArpEntry));
    memcpy((void*)Entry->Ip, Ip, 4);
    memcpy((void*)Entry->Mac, Mac, 6);
    Entry->Next = *Table;
    *Table = Entry;
    return KSUCCESS;
}

NetArpEntry* NetArpTableResolve(NetArpEntry** Table, uint8_t* Ip) {
    NetArpEntry* Current = *Table;
    while (Current != NULL) {
        if (memcmp((void*)Current->Ip, Ip, 4) == 0) {
            return Current;
        }
        Current = Current->Next;
    }
    uint8_t Broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    KSTATUS r = NetArpRequest(NetGetLinkedList(), Broadcast, KernelGetInformation()->net.Mac, Ip, KernelGetInformation()->net.Ip);
    if (r == KSUCCESS) {
        int Timeout = 0;
        NetArpEntry* NTable = KernelGetInformation()->net.ArpHead;
        NetArpEntry* Current2 = NTable;
        while (Current2 != NULL && Timeout < 1000000) {
            if (memcmp((void*)Current2->Ip, Ip, 4) == 0) return Current2;
            Current2 = Current2->Next;
        }
        printf("net: resolve: timeout reached!\r\n");
    }
    return NULL;
}
// should be called by network driver on every packet
// so we can see if its an ARP request
KSTATUS NetHandlePacket(NetInterface* Nic, void* Buffer, uint16_t Length) {
    if (!Buffer || Length == 0) return KINVALID;
    if (!Nic) return KINVALID;
    NetEthFrameHdr* EthFrame = (NetEthFrameHdr*)Buffer;
    // printf("net: Packet from [");
    // UtilPrintMacAddr(EthFrame->SrcMac);
    // printf("] to [");
    // UtilPrintMacAddr(EthFrame->DestMac);
    // printf("]\r\n");
    if (UtilSwapEnd16(EthFrame->EtherType) == NET_ETHTYPE_ARP) {
        // is arp packet
        NetArpHdr* ArpHdr = (NetArpHdr*)((uint64_t)EthFrame + sizeof(NetEthFrameHdr));
        // printf("net: Packet is ARP packet.\r\n");
        // if (UtilSwapEnd16(ArpHdr->Opcode) == 1) {
        //     printf("net: Opcode type is Request.\r\n");
        // } else if (UtilSwapEnd16(ArpHdr->Opcode) == 2) {
        //     printf("net: Opcode type is Reply.\r\n");
        // }
        #ifdef _NET_DEBUG
        printf("net: ARP Packet from [%d.%d.%d.%d] to [%d.%d.%d.%d]\r\n", ArpHdr->SrcPr[0], ArpHdr->SrcPr[1], ArpHdr->SrcPr[2], ArpHdr->SrcPr[3], ArpHdr->DestPr[0], ArpHdr->DestPr[1], ArpHdr->DestPr[2], ArpHdr->DestPr[3]);
        #endif
        // send a reply
        if (KernelGetInformation()->net.Ip[0] == 0) {
            printf("net: err: our ip hasn't been initalized (Ip[0] = 0)\r\n");
            return KUNSUPPORTED;
        }
        int IsIp = memcmp((void*)ArpHdr->DestPr, (void*)KernelGetInformation()->net.Ip, 4);
        if (UtilSwapEnd16(ArpHdr->Opcode) == 1 && (IsIp == 0)) {
            KSTATUS r2 = NetArpReply(Nic, ArpHdr->SrcHw, Nic->MacAddress, ArpHdr->SrcPr, ArpHdr->DestPr);
            if (r2 != KSUCCESS) {
                printf("net: warn: replying to arp request failed!\r\n");
            }
        }
        if (UtilSwapEnd16(ArpHdr->Opcode) == 2 && (IsIp == 0)) {
            KSTATUS r2 = NetArpTableAdd(&KernelGetInformation()->net.ArpHead, EthFrame->SrcMac, ArpHdr->SrcPr);
            if (r2 != KSUCCESS) {
                printf("net: warn: adding mapping for mac -> ip failed\r\n");
            }
        }
        return KSUCCESS;
    } else if (UtilSwapEnd16(EthFrame->EtherType) == NET_ETHTYPE_IPV4) {
        // printf("net: Packet is IP packet.\r\n");
        NetIpv4Hdr* Ipv4Hdr = (NetIpv4Hdr*)((uint64_t)EthFrame + sizeof(NetEthFrameHdr));
        // printf("net: IP version: %d\r\n", Ipv4Hdr->Version);
        // printf("net: IHL %d\r\n", Ipv4Hdr->InternetHdrLength);
        // printf("net: Total packet size: %d\r\n", UtilSwapEnd16(Ipv4Hdr->Length));
        // printf("net: TTL: %d\r\n", Ipv4Hdr->Ttl);
        // printf("net: Protocol number: %d\r\n", Ipv4Hdr->Protocol);
        // printf("net: [%d.%d.%d.%d] -> [%d.%d.%d.%d]\r\n", Ipv4Hdr->Sender[0], Ipv4Hdr->Sender[1], Ipv4Hdr->Sender[2], Ipv4Hdr->Sender[3], Ipv4Hdr->Destination[0], Ipv4Hdr->Destination[1], Ipv4Hdr->Destination[2], Ipv4Hdr->Destination[3]);
        KSTATUS r = NetIpv4Handle(EthFrame, Ipv4Hdr);
        // printf("net: NetIpv4Handle KSTATUS 0x%lx\r\n", r);
    }
    // note we dont free the buffer mainly cuz the proper NetRead call might still be using it
    // so we should let that free it
    return KUNSUPPORTED;
}
KE_EXPORT_SYMBOL(NetHandlePacket);