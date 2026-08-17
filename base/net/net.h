#pragma once
#include <stdint.h>
#include <kernel.h>
#include <kedriver.h>
#include <arch/x86_64/cpu/paging.h> // so much for arch compat
#include <sched/process.h>
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

KSTATUS NetRegisterNic(NetInterface* Nic);
NetInterface* NetGetLinkedList();
KSTATUS NetWriteRaw(NetInterface* Nic, void* Buffer, uint16_t Length);
KSTATUS NetReadRaw(NetInterface* Nic, void* Buffer, uint16_t Length, uint16_t* BytesReadOut);