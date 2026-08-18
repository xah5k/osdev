#pragma once
#include <stdint.h>
#include <sched/process.h>
#include <kedriver.h>
#include <arch/x86_64/cpu/paging.h> // so much for arch compat
typedef struct NetInterface {
    char Name[64];
    uint8_t MacAddress[6];
    uint64_t IoBase;
    KeDeviceObj* Device;
    physaddr RxBuffer;
    physaddr TxBuffer; // if applicable
    uint8_t NextTxDesc; // if applicable
    ThreadCtrlBlk* RxWaitListHead; // if applicable which it should be
    ThreadCtrlBlk* RxWaitListTail; // if applicable which it should be
    void* DriverState;
    struct NetInterface* Next;
} NetInterface;

typedef struct {
    uint8_t DestMac[6];
    uint8_t SrcMac[6];
    uint16_t EtherType;
} NetEthFrameHdr;

// used in the kernel as an ARP table
typedef struct NetArpEntry {
    uint8_t Mac[6];
    uint8_t Ip[4];
    struct NetArpEntry* Next;
} NetArpEntry;

typedef struct {
    uint16_t HType;
    uint16_t PType;
    uint8_t HwAddrLen; // 6 = ethernet
    uint8_t PrAddrLen; // 4 = ipv4
    uint16_t Opcode;
    uint8_t SrcHw[6]; // todo all of this src and dest assumes HwAddrLen being 6 and PrAddrLen being 4. if it isnt theres gonna be issues 
    uint8_t SrcPr[4];
    uint8_t DestHw[6];
    uint8_t DestPr[4];
} NetArpHdr;

typedef struct {
    uint8_t InternetHdrLength : 4;
    uint8_t Version : 4;
    uint8_t Ecn : 2;
    uint8_t Dscp : 6;
    uint16_t Length;
    uint16_t Id;
    uint8_t Flags : 3;
    uint16_t FragmentOff : 13;
    uint8_t Ttl;
    uint8_t Protocol;
    uint16_t HdrChecksum;
    uint8_t Sender[4];
    uint8_t Destination[4];
    // options comes after but like no one uses them nowdays
} __attribute__((packed)) NetIpv4Hdr;

typedef struct {
    uint8_t Type;
    uint8_t Code;
    uint16_t Checksum;
}  __attribute__((packed)) NetIcmpHdr;

typedef struct {
    uint16_t Id;
    uint16_t Sequence;
}  __attribute__((packed)) NetIcmpEchoHdr;

// defines
#define NET_ETHTYPE_ARP 0x0806
#define NET_ETHTYPE_IPV4 0x0800
#define NET_IPV4_PROTOCOL_ICMP 1

KSTATUS NetRegisterNic(NetInterface* Nic);
NetInterface* NetGetLinkedList();
KSTATUS NetWriteRaw(NetInterface* Nic, void* Buffer, uint16_t Length);
KSTATUS NetReadRaw(NetInterface* Nic, void* Buffer, uint16_t Length, uint16_t* BytesReadOut);
KSTATUS NetArpReply(NetInterface* Nic, uint8_t* ToMacAddress, uint8_t* FromMacAddress, uint8_t* ToIpAddress, uint8_t* FromIpAddress);
KSTATUS NetHandlePacket(NetInterface* Nic, void* Buffer, uint16_t Length);
KSTATUS NetArpRequest(NetInterface* Nic, uint8_t* ToMacAddress, uint8_t* FromMacAddress, uint8_t* ToIpAddress, uint8_t* FromIpAddress);
NetArpEntry* NetArpTableResolve(NetArpEntry** Table, uint8_t* Ip);
KSTATUS NetIcmpEchoRequest(NetInterface* Nic, uint8_t* ToIpAddress, uint16_t Sequence);
typedef enum {
    NET_CALLBACK_ICMP
} NetCallbackType;

typedef struct {
    KSTATUS(*CallBack)(NetEthFrameHdr* EFrame, NetIpv4Hdr* Ipv4, NetIcmpEchoHdr* Echo, NetIcmpHdr* Icmp);
    NetCallbackType Type;
} NetCallback;


KSTATUS NetRegisterCallback(uint16_t At, NetCallback* callback);
KSTATUS NetDeregisterCalback(uint16_t At);