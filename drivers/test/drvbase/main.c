#include <kernel.h>
#include <kedriver.h>
#include <fb.h>
#include <fs/vfs.h>
#include <mm/heap.h>
KSTATUS DriverEntry(KeDriverObj* Self) {
    KeDrvWrite("hello world from driver!\r\n");
    KeDrvWriteFmt("self = 0x%lx self->init = 0x%lx\r\n", Self, Self->Initalize);
    return KSUCCESS;
}