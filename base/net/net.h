#pragma once
#include <stdint.h>
#include <kernel.h>
#include <kedriver.h>
typedef struct NetInterface {
    char Name[64];
    uint8_t MacAddress[6];
    uint64_t IoBase;
    KeDeviceObj* Device;
    struct NetInterface* Next;
} NetInterface;

KSTATUS NetRegisterNic(NetInterface* Nic);
NetInterface* NetGetLinkedList();