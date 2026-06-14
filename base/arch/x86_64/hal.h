#pragma once
#include <stdint.h>
#include <kernel.h>
#define HAL_DEVICE_TYPE_INPUT 0
#define HAL_DEVICE_TYPE_TIMER 1

typedef struct {
	int OsId; // internal os identifier
	uint64_t IntVector; // interrupt vector for where it's handler is
	char Type;
} HalDevice;

typedef struct {
	HalDevice* Header;
	char (*ReadCode)(); // read a scancode from the input device
} HalInputDevice;

typedef struct {
	HalDevice* Header;
	uint64_t(*GetTick)(); // return current tick the timer is at
} HalTimerDevice;

void HalRegisterDevice(HalDevice* device);
void HalInitalize(KernelInformation* kinfo);

void HalDbgListDevices();