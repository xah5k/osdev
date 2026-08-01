#pragma once
#include <kernel.h>
#include <kedriver.h>
KSTATUS LdrElfExecute(void* addr, uint8_t priv, uint64_t* pidout, const char** argv, int argc, const char** envp, int envc, const char* name);
KSTATUS LdrElfDriverExec(void* addr, KeDriverObj** driver);