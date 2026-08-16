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

KSTATUS NetWriteRaw(NetInterface* Nic) {
    KeIoRequest Irp;
    char* Buffer = "some stupid garbage";
    Irp.Major = IO_WRITE;
    Irp.Buffer = Buffer;
    Irp.Length = 20;
    Irp.ReadBytes = 0;
    KSTATUS r = KeIoDispatch(Nic->Device, &Irp);
    return r;
}