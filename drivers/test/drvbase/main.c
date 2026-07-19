#include <kernel.h>
#include <kedriver.h>
KSTATUS DriverEntry(KeDriverObj* Self) {
    KeDrvWrite("hello world from driver!\r\n");
    return KSUCCESS;
}