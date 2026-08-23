#include <kernel.h>
#include <kedriver.h>
#include <memory.h>
#include <arch/x86_64/cpu/idt.h>
#include <arch/x86_64/cpu/lapic.h>
#include <arch/x86_64/cpu/ioapic.h>
#include <arch/x86_64/ports.h>
#include <mm/heap.h>
#include <sched/process.h>
#include <sched/sched.h>
#include <mouse.h>

#define PS2LEFTBTN 0b00000001
#define PS2MIDDLEBTN 0b00000100
#define PS2RIGHTBTN 0b00000010
#define PS2XSIGN 0b00010000
#define PS2YSIGN 0b00100000
#define PS2XOVERFLW 0b01000000
#define PS2YOVERFLW 0b10000000

static void MouseHandle(uint8_t Data);

static void MouseWait() {
    uint64_t timeout = 100000;
    while (timeout--) {
        if ((inb(0x64) & 0b10) == 0) return;
    }
}
static void MouseWaitForInput() {
    uint64_t timeout = 100000;
    while (timeout--) {
        if (inb(0x64) & 0b1) return;
    }
}

void MouseInterruptHandler(CpuInterruptArgs* r) {
    uint8_t status = inb(0x64);
    if (status & 0x01) {
        uint8_t In = inb(0x60);
        MouseHandle(In);
    }
    CpuLapicEoi();
}
uint8_t MouseCy = 0;
uint8_t MousePck[4];
int MousePckReady = 0;
static void MouseHandle(uint8_t Data) {
    switch (MouseCy) {
        case 0: {
            if (MousePckReady) break;
            if ((Data & 0b00001000) == 0) break; // somethings gone wrong should always be 1
            MousePck[0] = Data;
            MouseCy++;
            break;
        }
        case 1:
            if (MousePckReady) break;
            MousePck[1] = Data;
            MouseCy++;
            break;
        case 2:
            if (MousePckReady) break;
            MousePck[2] = Data;
            MouseCy = 0;
            MousePckReady = 1;
            break;
    }
}

static void MouseProcessPacket() {
    if (!MousePckReady) return;
    MousePckReady = 0;
    KeDevMousePacket* KeMousePck = MmAllocate(sizeof(KeDevMousePacket));
    memset((void*)KeMousePck, 0, sizeof(KeDevMousePacket));
    if (!KeMousePck) KdBugcheck2(KERNEL_CORE_COMP_FAIL, NULL, __LINE__, __FILE__);
    int XNeg, YNeg, XOver, YOver;
    XNeg = (MousePck[0] & PS2XSIGN) ? 1 : 0;
    YNeg = (MousePck[0] & PS2YSIGN) ? 1 : 0;
    XOver = (MousePck[0] & PS2XOVERFLW) ? 1 : 0;
    YOver = (MousePck[0] & PS2YOVERFLW) ? 1 : 0; 
    if (!XNeg) {
        KeMousePck->RawPos.x += MousePck[1];
        if (XOver) KeMousePck->RawPos.x += 255;
    } else {
        MousePck[1] = 256 - MousePck[1];
        KeMousePck->RawPos.x -= MousePck[1];
        if (XOver) KeMousePck->RawPos.x -= 255;
    }
    if (!YNeg) {
        KeMousePck->RawPos.y -= MousePck[2];
        if (YOver) KeMousePck->RawPos.y -= 255;
    } else {
        MousePck[2] = 256 - MousePck[2];
        KeMousePck->RawPos.y += MousePck[2];
        if (YOver) KeMousePck->RawPos.y += 255;
    }
    if (MousePck[0] & PS2LEFTBTN) KeMousePck->LeftClickPress = 1;
    if (MousePck[0] & PS2RIGHTBTN) KeMousePck->RightClickPress = 1;
    if (MousePck[0] & PS2MIDDLEBTN) KeMousePck->MiddleClickPress = 1;
    KeDevSetMousePck(KeMousePck);
    // KeDrvWriteFmt("rawmat %d, %d\r\n", KeMousePck->RawPos.x, KeMousePck->RawPos.y);
}

static void MouseWritePort(uint8_t Value) {
    MouseWait();
    outb(0x64, 0xD4);
    MouseWait();
    outb(0x60, Value);
}

static uint8_t MouseReadPort() {
    MouseWaitForInput();
    return inb(0x60);
}

KSTATUS MouseHwSpec(KeDeviceObj* dev, KeIoRequest* irp) {
    MouseProcessPacket();
    return KSUCCESS;
}

void MouseInitalize() {
    outb(0x64, 0xA8);
    MouseWait();
    outb(0x64, 0x20);
    MouseWaitForInput();
    uint8_t Status = inb(0x60);
    Status |= 0b10;
    Status &= ~0b100000;
    MouseWait();
    outb(0x64, 0x60);
    MouseWait();
    outb(0x60, Status);
    MouseWritePort(0xF6);
    MouseReadPort();
    MouseWritePort(0xF4);
    MouseReadPort();
}

KSTATUS DriverEntry(KeDriverObj* Self) {
    memcpy(Self->Name, "mouse", 6);
    KeDrvWrite("mouse: initalizing!\r\n");
    MouseInitalize();
    uint64_t entry = 0;
    uint64_t dest = CpuLapicGetId();
    entry |= (dest << 56);

    entry |= 0x74;
    CpuIoApicSetRedirEntry(CpuIoApicTranslateIrq(12), entry);
    CpuRegisterHandler(0x74, MouseInterruptHandler);
    
    // create device object
    KeDeviceObj* device = MmAllocate(sizeof(KeDeviceObj));
    memset((void*)device, 0, sizeof(KeDeviceObj));
    memcpy(device->Name, "ps2mouse", 9);
    device->Owner = Self;
    device->Device = NULL;
    device->Next = NULL;
    device->Dispatch[IO_HWSPEC] = MouseHwSpec;
    KeRegisterDevice(device);
    KeDrvWrite("mouse: registered device as 'ps2mouse'\r\n");
    return KSUCCESS;
}