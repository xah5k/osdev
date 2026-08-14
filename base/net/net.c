#include <net/net.h>
#include <kedriver.h>
#include <stddef.h>

// list of netinterface structs
static NetInterface* gNetInterfaceHead = NULL;

NetInterface* NetGetLinkedList() {
    return gNetInterfaceHead;
}

KSTATUS NetRegisterNic(NetInterface* Nic) {
    Nic->Next = gNetInterfaceHead;
    gNetInterfaceHead = Nic;    
    return KSUCCESS;
}

KE_EXPORT_SYMBOL(NetRegisterNic);