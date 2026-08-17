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


KSTATUS NetRegisterNic(NetInterface* Nic);
NetInterface* NetGetLinkedList();
KSTATUS NetWriteRaw(NetInterface* Nic, void* Buffer, uint16_t Length);
KSTATUS NetReadRaw(NetInterface* Nic, void* Buffer, uint16_t Length, uint16_t* BytesReadOut);
KSTATUS NetArpReply(NetInterface* Nic, uint8_t* ToMacAddress, uint8_t* FromMacAddress, uint8_t* ToIpAddress, uint8_t* FromIpAddress);
KSTATUS NetHandlePacket(NetInterface* Nic, void* Buffer, uint16_t Length);