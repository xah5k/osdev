#pragma once
#include <kernel.h>
#include <kedriver.h>
#include <external/elf.h>
#include <sched/process.h>
KSTATUS LdrElfReplaceImage(ProcessCtrlBlk* target, void* image, const char** argv, int argc, const char** envp, int envc);
void LdrElfMapPhdr(Elf64_Phdr* PHdr, Elf64_Ehdr* Elf, ProcessCtrlBlk* proc);
KSTATUS LdrElfExecute(void* addr, uint8_t priv, uint64_t* pidout, const char** argv, int argc, const char** envp, int envc, const char* name);
KSTATUS LdrElfDriverExec(void* addr, KeDriverObj** driver);