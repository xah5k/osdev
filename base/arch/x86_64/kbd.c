#include "kbd.h"
#include "arch/x86_64/cpu/idt.h"
#include "cpu/ioapic.h"
#include "cpu/lapic.h"
#include <printfwrapper.h>
#include "ports.h"

char KbdReadCode() {
	return inb(0x60);
}

void KbdInterruptHandler(CpuInterruptArgs* r) {
    uint8_t x = inb(0x60);
    printf("kbd: dbg: 0x%x\r\n", x);
    CpuLapicEoi();
}

void KbdInitalize(virtaddr ioapicbase, uint64_t ivector) {
    printf("kbd: initalizing\r\n");

    uint64_t entry = 0;
    uint64_t dest = CpuLapicGetId();
    entry |= (dest << 56);

    entry |= 0x21;

    CpuIoApicSetRedirEntry(1, entry);
    CpuRegisterHandler(ivector, KbdInterruptHandler);
    printf("kbd: enabled interrupt\r\n");
}