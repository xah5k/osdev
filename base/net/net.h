#pragma once
#include <stdint.h>
#include <sched/process.h>
#include <kedriver.h>
#include <hal/mmu.h>
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

// ARP
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

// IPv4
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

// ICMP
typedef struct {
    uint8_t Type;
    uint8_t Code;
    uint16_t Checksum;
}  __attribute__((packed)) NetIcmpHdr;

typedef struct {
    uint16_t Id;
    uint16_t Sequence;
}  __attribute__((packed)) NetIcmpEchoHdr;

// UDP
typedef struct {
    uint16_t SrcPort;
    uint16_t DestPort;
    uint16_t Length;
    uint16_t Checksum;
    // payload comes after
} __attribute__((packed)) NetUdpHdr;

// DHCP
// https://en.wikipedia.org/wiki/Dynamic_Host_Configuration_Protocol#Discovery
// this shi was so confusing at first but maybe im just retarded

typedef struct {
    uint8_t Type;
    uint8_t Length;
    // value comes after in Length bytes
} __attribute__((packed)) NetDhcpOption;

typedef struct {
    uint8_t Opcode;
    uint8_t HType;
    uint8_t HLen;
    uint8_t HOps;
    uint32_t XId;
    uint16_t Secs;
    uint16_t Flags;
    uint8_t CiAddr[4]; // client ip
    uint8_t YiAddr[4]; // your ip
    uint8_t SiAddr[4]; // server addr
    uint8_t GiAddr[4]; // gateway address
    uint8_t ChHwAddr[16]; // client hardware address
    uint8_t LegacyBootp[192];
    uint32_t MagicCookie;
    NetDhcpOption Options[];
} __attribute__((packed)) NetDhcpHdr;

// defines
#define NET_ETHTYPE_ARP 0x0806
#define NET_ETHTYPE_IPV4 0x0800
#define NET_IPV4_PROTOCOL_ICMP 1
#define NET_IPV4_PROTOCOL_UDP 17

#define NET_DHCP_OPTION_GET8(opt) *(uint8_t*)((uint64_t)opt + sizeof(NetDhcpOption))
#define NET_DHCP_OPTION_GET(opt, type) *(type*)((uint64_t)opt + sizeof(NetDhcpOption))
#define NET_DHCP_OPTION_GETOFF(opt, type, offset) *(type*)((uint64_t)opt + sizeof(NetDhcpOption) + offset)
#define NET_DHCP_OPTION_GETPTR(opt) (uint8_t*)((uint64_t)opt + sizeof(NetDhcpOption))

KSTATUS NetRegisterNic(NetInterface* Nic);
NetInterface* NetGetLinkedList();
KSTATUS NetWriteRaw(NetInterface* Nic, void* Buffer, uint16_t Length);
KSTATUS NetReadRaw(NetInterface* Nic, void* Buffer, uint16_t Length, uint16_t* BytesReadOut);
KSTATUS NetArpReply(NetInterface* Nic, uint8_t* ToMacAddress, uint8_t* FromMacAddress, uint8_t* ToIpAddress, uint8_t* FromIpAddress);
KSTATUS NetHandlePacket(NetInterface* Nic, void* Buffer, uint16_t Length);
KSTATUS NetArpRequest(NetInterface* Nic, uint8_t* ToMacAddress, uint8_t* FromMacAddress, uint8_t* ToIpAddress, uint8_t* FromIpAddress);
NetArpEntry* NetArpTableResolve(NetArpEntry** Table, uint8_t* Ip);
KSTATUS NetIcmpEchoRequest(NetInterface* Nic, uint8_t* ToIpAddress, uint16_t Sequence);
KSTATUS NetUdpSend(NetInterface* Nic, uint8_t* ToIpAddress, void* Payload, uint16_t Length, uint16_t SrcPort, uint16_t DestPort);
KSTATUS NetArpTableAdd(NetArpEntry** Table, uint8_t* Mac, uint8_t* Ip);
KSTATUS NetDhcpDiscover(NetInterface* Nic);
KSTATUS NetDhcpConfigure(NetInterface* Nic);

typedef enum {
    NET_CALLBACK_ICMP,
    NET_CALLBACK_UDP
} NetCallbackType;

typedef struct {
    uint16_t Port;
    KSTATUS(*CallBack)(NetEthFrameHdr* EFrame, NetIpv4Hdr* Ipv4, NetIcmpEchoHdr* Echo, NetIcmpHdr* Icmp, NetUdpHdr* Udp);
    NetCallbackType Type;
} NetCallback;


KSTATUS NetRegisterCallback(uint16_t At, NetCallback* callback);
KSTATUS NetDeregisterCalback(uint16_t At);