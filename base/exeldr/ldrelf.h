#pragma once
#include <kernel.h>
#include <kedriver.h>
KSTATUS LdrElfExecute(void* addr);
KSTATUS LdrElfDriverExec(void* addr, KeDriverObj** driver);