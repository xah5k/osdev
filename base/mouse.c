#include <mouse.h>
#include <printfwrapper.h>
#include <kedriver.h>
static KeDevMousePacket* gPacket = NULL;
void KeDevSetMousePck(KeDevMousePacket* packet) {
    gPacket = packet;
}
KE_EXPORT_SYMBOL(KeDevSetMousePck);
KSTATUS KeDevMouseProcess() {
    KeDeviceObj* d = KeFindDeviceByName("ps2mouse");
    if (!d) return KINVALID;
    while (1) { 
        d->Dispatch[IO_HWSPEC](NULL, NULL);    
        if (gPacket) {
            UtilPrintFmtAt("mouse: %.2d,%.2d  L:%.2d  M:%.2d  R:%.2d  ", 150, 150, gPacket->RawPos.x, gPacket->RawPos.y, gPacket->LeftClickPress, gPacket->MiddleClickPress, gPacket->RightClickPress);
        }
    }
    return KSUCCESS;
}