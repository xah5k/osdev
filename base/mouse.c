#include <mouse.h>
#include <printfwrapper.h>
#include <kedriver.h>
#include <fb.h>
#include <memory.h>
#include <mm/heap.h>
#include <fs/vfs.h>

static KeDevMousePacket* gPacket = NULL;
void KeDevSetMousePck(KeDevMousePacket* packet) {
    gPacket = packet;
}
KE_EXPORT_SYMBOL(KeDevSetMousePck);