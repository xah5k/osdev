#include <arch/x86_64/hal.h>
#include <mm/heap.h>
#include <arch/x86_64/cpu/lapic.h>
#include <arch/x86_64/cpu/ioapic.h>
#include <arch/x86_64/kbd.h>
#include <stddef.h>
#include <external/printf.h>
#include <memory.h>
#define HAL_INITIAL_MAX_DEVICES 32
#define HAL_DEVLIST_ALC HAL_INITIAL_MAX_DEVICES * (sizeof(HalDevice) + 8)
HalDevice* gDevList;
static uint64_t index = 0;

void HalInitalize() {
	// note the better method should be to actually detect which devices are connected and register them and their device driver based on that
	// but for now assume there is a ps/2 keyboard and a lapic timer
	gDevList = MmAllocate(HAL_DEVLIST_ALC);
	memset(gDevList, 0, HAL_DEVLIST_ALC);

	// initalize lapic timer
	HalTimerDevice* LapicTimer = MmAllocate(sizeof(HalTimerDevice));
	LapicTimer->Header->OsId = 0;
	LapicTimer->Header->IntVector = 32;
	LapicTimer->Header->Type = HAL_DEVICE_TYPE_TIMER;
	LapicTimer->GetTick = CpuLapticTimerGetTick;
	CpuInitalizeLapicTimer(LapicTimer->Header->IntVector);
	gDevList = (HalDevice*)LapicTimer;
	index++;
	// initalize keyboard
	HalInputDevice* KbdDev = MmAllocate(sizeof(HalInputDevice));
	KbdDev->Header->OsId = 1;
	KbdDev->Header->IntVector = 33;
	KbdDev->Header->Type = HAL_DEVICE_TYPE_INPUT;
	KbdDev->ReadCode = KbdReadCode;
	KbdInitalize(CpuGetIoApicVirtBase(), KbdDev->Header->IntVector);
	*(volatile uint64_t*)(((uint64_t)gDevList + (sizeof(HalDevice) + 8)) * index) = (uint64_t)KbdDev;
}

void HalRegisterDevice(HalDevice* device) {
	*(volatile uint64_t*)(((uint64_t)gDevList + (sizeof(HalDevice) + 8)) * index) = (uint64_t)device;
	index++;
}

void HalDbgListDevices() {
	HalDevice* current = gDevList;
	while (current != NULL) {
		switch (current->Type) {
		case HAL_DEVICE_TYPE_INPUT:
			HalInputDevice* device = (HalInputDevice*)current;
			printf("hal: Found input device in list.(devlist starts @ 0x%lx this entry is @ 0x%lx)\r\n", gDevList, current);
			printf("hal: os specific id: %d\r\nhal: interupt vector: %d\r\n", device->Header->OsId, device->Header->IntVector);
			printf("hal: device specific functions: \r\n");
			printf("hal: ReadCode @ 0x%lx\r\n", device->ReadCode);
			break;
		case HAL_DEVICE_TYPE_TIMER:
			HalTimerDevice* device2 = (HalTimerDevice*)current;
			printf("hal: Found timer device in list.(devlist starts @ 0x%lx this entry is @ 0x%lx)\r\n", gDevList, current);
			printf("hal: os specific id: %d\r\nhal: interupt vector: %d\r\n", device2->Header->OsId, device2->Header->IntVector);
			printf("hal: device specific functions: \r\n");
			printf("hal: GetTick @ 0x%lx\r\n", device2->GetTick);
			break;
		default:
			printf("hal: Unknown device in device list (devlist starts @ 0x%lx this entry is @ 0x%lx)\r\n", gDevList, current);
			printf("hal: os specific id: %d\r\n", current->OsId);
			break;
		}
		current += (sizeof(HalDevice) + 8);
	}
}