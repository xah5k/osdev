#pragma once
#include <stdint.h>
#include <kernel.h>
#include <kedriver.h>
#include <arch/x86_64/cpu/paging.h> // so much for arch compat
typedef struct NetInterface {
    char Name[64];
    uint8_t MacAddress[6];
    uint64_t IoBase;
    KeDeviceObj* Device;
    physaddr RxBuffer;
    physaddr TxBuffer; // if applicable
    struct NetInterface* Next;
} NetInterface;

KSTATUS NetRegisterNic(NetInterface* Nic);
NetInterface* NetGetLinkedList();