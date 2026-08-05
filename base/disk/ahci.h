#pragma once
#include <arch/x86_64/pci/pci.h>
#include <kedriver.h>

KeDriverObj* AhciInitalize(PciDeviceHeader* PciBase);
