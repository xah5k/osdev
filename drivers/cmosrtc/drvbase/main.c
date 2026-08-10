#include <kernel.h>
#include <kedriver.h>
#include <fb.h>
#include <fs/vfs.h>
#include <mm/heap.h>
#include <arch/x86_64/ports.h>
#include <memory.h>

// most code here is based off https://wiki.osdev.org/CMOS

static int RtcGetPrgFlag() {
    outb(0x70, 0x0A);
    return (inb(0x71) & 0x80);
}

static uint8_t RtcGetReg(int Reg) {
    outb(0x70, Reg);
    return inb(0x71);
}

static KSTATUS RtcPopulateStruct(KeDevClockWallTime* WalltimeOut) {
    if (!WalltimeOut) return KINVALID;
    uint8_t CSecond;
    uint8_t CMinute;
    uint8_t CHour;
    uint8_t CDay;
    uint8_t CMonth;
    uint8_t CYear;
    uint8_t RegB;
    uint8_t LSecond;
    uint8_t LMinute;
    uint8_t LHour;
    uint8_t LDay;
    uint8_t LMonth;
    uint8_t LYear;
    while (RtcGetPrgFlag());
    CSecond = RtcGetReg(0x00);
    CMinute = RtcGetReg(0x02);
    CHour = RtcGetReg(0x04);
    CDay = RtcGetReg(0x07);
    CMonth = RtcGetReg(0x08);
    CYear = RtcGetReg(0x09);
    do {
        LSecond = CSecond;
        LMinute = CMinute;
        LHour = CHour;
        LDay = CDay;
        LMonth = CMonth;
        LYear = CYear;
        while (RtcGetPrgFlag());
        CSecond = RtcGetReg(0x00);
        CMinute = RtcGetReg(0x02);
        CHour = RtcGetReg(0x04);
        CDay = RtcGetReg(0x07);
        CMonth = RtcGetReg(0x08);
        CYear = RtcGetReg(0x09);
    } while ((LSecond != CSecond) || (LMinute != CMinute) || (LHour != CHour) || (LDay != CDay) || (LMonth != CMonth) || (LYear != CYear));
    RegB = RtcGetReg(0x0B);
    if (!(RegB & 0x04)) {
        CSecond = (CSecond & 0x0F) + ((CSecond / 16) * 10);
        CMinute = (CMinute & 0x0F) + ((CMinute / 16) * 10);
        CHour = ((CHour & 0x0F) + (((CHour & 0x70) / 16) * 10)) | (CHour & 0x80);
        CDay = (CDay & 0x0F) + ((CDay / 16) * 10);
        CMonth = (CMonth & 0x0F) + ((CMonth / 16) * 10);
        CYear = (CYear & 0x0F) + ((CYear / 16) * 10);
    }
    if (!(RegB & 0x02) && (CHour & 0x80)) {
        CHour = ((CHour & 0x7F) + 12) % 24;
    }
    WalltimeOut->Seconds = CSecond;
    WalltimeOut->Minutes = CMinute;
    WalltimeOut->Hours = CHour;
    WalltimeOut->Days = CDay;
    WalltimeOut->Month = CMonth;
    WalltimeOut->Year = CYear;
    return KSUCCESS;
}

KSTATUS RtcHwSpecPopulate(KeDeviceObj* dev, KeIoRequest* irp) {
    if (!dev || !irp) return KINVALID;
    KSTATUS r = RtcPopulateStruct(irp->Buffer);
    irp->ReadBytes = sizeof(KeDevClockWallTime);
    return r;
}

KSTATUS DriverEntry(KeDriverObj* Self) {
    memcpy(Self->Name, "cmosrtc", 8);
    KeDeviceObj* device = MmAllocate(sizeof(KeDeviceObj));
    memcpy(device->Name, "rtc", 7);
    device->Owner = Self;
    device->Device = NULL;
    device->Next = NULL;
    device->Dispatch[IO_HWSPEC + KE_WALLTIME_HWSPEC_OFF] = RtcHwSpecPopulate;
    KeRegisterDevice(device);
    return KSUCCESS;
}