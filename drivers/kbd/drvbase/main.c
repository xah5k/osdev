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
#define KBD_BUFFER_SIZE 256

typedef struct {
    uint8_t buffer[KBD_BUFFER_SIZE];
    uint32_t head;
    uint32_t tail;
} KbdRingBuffer;

static KbdRingBuffer gKbdBuf = {0};
ThreadCtrlBlk* gKbdWaitQueueHead;
ThreadCtrlBlk* gKbdWaitQueueTail;
static int KbdBufferAdd(uint8_t scancode) {
    uint32_t next = (gKbdBuf.head + 1) % KBD_BUFFER_SIZE;
    if (next == gKbdBuf.tail) {
        return 0;
    }
    gKbdBuf.buffer[gKbdBuf.head] = scancode;
    gKbdBuf.head = next;
    return 1;
}

static int KbdBufferRm(uint8_t* out_scancode) {
    if (gKbdBuf.head == gKbdBuf.tail) {
        return 0;
    }
    *out_scancode = gKbdBuf.buffer[gKbdBuf.tail];
    gKbdBuf.tail = (gKbdBuf.tail + 1) % KBD_BUFFER_SIZE;
    return 1;
}

char KbdReadCode() {
	return inb(0x60);
}

void KbdInterruptHandler(CpuInterruptArgs* r) {
    uint8_t x = KbdReadCode();
    KbdBufferAdd(x);
    // KeDrvWriteFmt("kbd: buffer add scancode 0x%x\r\n", x);
    ThreadCtrlBlk* SuspendedThr = ThreadPopHead(&gKbdWaitQueueHead, &gKbdWaitQueueTail);
    // KeDrvWriteFmt("kbd: suspendedthr @ 0x%lx\r\n", SuspendedThr);
    if (SuspendedThr != NULL) {
        // KeDrvWriteFmt("kbd driver: wake thread@0x%lx{tid=%d, parent pid=%d}\r\n", SuspendedThr, SuspendedThr->tid, SuspendedThr->ParentProc->pid);
        ThreadWake(SuspendedThr);
    }
    CpuLapicEoi();
}

KSTATUS KbdRead(KeDeviceObj* dev, KeIoRequest* irp) {
    if (!dev) return KINVALID;
    if (!irp) return KINVALID;
    if (irp->Major != IO_READ) return KINVALID;
    if (!irp->Buffer || irp->Length == 0) return KINVALID;

    uint8_t* UserBuf = (uint8_t*)irp->Buffer;
    uint64_t BytesRead = 0;
    while (BytesRead < irp->Length) {
        uint8_t scancode;
        while (!KbdBufferRm(&scancode)) {
            ThreadCtrlBlk* cthr = ThrGetCurrent();
            // KeDrvWriteFmt("kbd driver: suspend thread{tid=%d, parent pid=%d}\r\n", cthr->tid, cthr->ParentProc->pid);
            cthr->state = SCHED_THREAD_SUSPENDED;
            ThreadPushTail(&gKbdWaitQueueHead, &gKbdWaitQueueTail, cthr);
            SchedYield();
        }
        UserBuf[BytesRead++] = scancode;
    }
    irp->ReadBytes = BytesRead;
    return KSUCCESS;
}

KSTATUS DriverEntry(KeDriverObj* Self) {
    memcpy(Self->Name, "kbd", 4);
    KeDrvWrite("kbd: initalizing!\r\n");
    uint64_t entry = 0;
    uint64_t dest = CpuLapicGetId();
    entry |= (dest << 56);

    entry |= 0x21;

    CpuIoApicSetRedirEntry(CpuIoApicTranslateIrq(1), entry);
    CpuRegisterHandler(33, KbdInterruptHandler);
    
    // create device object
    KeDeviceObj* device = MmAllocate(sizeof(KeDeviceObj));
    memcpy(device->Name, "ps2kbd", 7);
    device->Owner = Self;
    device->Device = NULL;
    device->Next = NULL;
    device->Dispatch[IO_READ] = KbdRead;
    device->Dispatch[IO_WRITE] = NULL;
    KeRegisterDevice(device);
    KeDrvWrite("kbd: registered device as 'ps2kbd'\r\n");
    return KSUCCESS;
}