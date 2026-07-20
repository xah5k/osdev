#pragma once
#include <kernel.h>
#include <kedriver.h>
KSTATUS LdrElfExecute(void* addr, uint8_t priv);
KSTATUS LdrElfDriverExec(void* addr, KeDriverObj** driver);