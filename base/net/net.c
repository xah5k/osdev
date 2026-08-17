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

KSTATUS NetWriteRaw(NetInterface* Nic, void* Buffer, uint16_t Length) {
    KeIoRequest Irp;
    Irp.Major = IO_WRITE;
    Irp.Buffer = Buffer;
    Irp.Length = Length;
    Irp.ReadBytes = 0;
    KSTATUS r = KeIoDispatch(Nic->Device, &Irp);
    return r;
}