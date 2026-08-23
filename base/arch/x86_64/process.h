#pragma once
#include <hal/mmu.h>
#include <stdint.h>
#include <sched/process.h>
uint64_t* ProcNewPML4();
void ProcFreeInnerPML4(pagetable* pml4p);
void ProcFreePML4(pagetable* pml4p);
void ThreadCreateKrnlStack(ThreadCtrlBlk* Tcb, void* entry);
void ThreadCreateUserStack(ThreadCtrlBlk* Tcb, void* entry, const char** argv, int argc, const char** envp, int envc);
void ThreadMapUserStack(ThreadCtrlBlk* Tcb);