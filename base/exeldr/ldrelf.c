#include "ldrelf.h"
#include <external/elf.h>
#include <memory.h>
#include <external/printf.h>
#include <mm/pmm.h>
#include <sched/process.h>
KSTATUS LdrElfExecute(void* addr) {
    Elf64_Ehdr* Elf = (Elf64_Ehdr*)addr;
    if (memcmp(Elf->e_ident, ELFMAG, 4) != 0) {
        printf("ldr: elf64: invalid magic\r\n");
        return KINVALID;
    }
    if ((Elf->e_ident[EI_CLASS] != ELFCLASS64) && (Elf->e_machine != EM_X86_64)) {
        printf("ldr: elf64: invalid cpu architecture\r\n");
        return KINVALID;
    }
    Elf64_Phdr* PHdr = (Elf64_Phdr*)((void*)Elf + Elf->e_phoff);
    if (Elf->e_phentsize != sizeof(Elf64_Phdr)) {
        printf("ldr: elf64: program header size mismatch!\r\n");
        return KINVALID;
    }
    ProcessCtrlBlk* proc = ProcessNew();
    for (int i = 0; i < Elf->e_phnum; i++) {
        Elf64_Phdr* current = &PHdr[i];
        if (current->p_type == PT_LOAD) {
            uint64_t filesz = current->p_filesz;
            uint64_t memsz = current->p_memsz;
            uint64_t offset = current->p_vaddr & 4095;
            void* vaddr = (void*)((uint64_t)current->p_vaddr - offset);
            int pages = (current->p_memsz + offset + 4095) / 4096;
            void* block = PmmAllocatePages(pages);
            for (int p = 0; p < pages; p++) {
                // lwk i should seperate the _x86_64_get_pml4 into like a ArchGetPageTable() that returns a virtual address
                MmuMapPage((pagetable*)((uint64_t)proc->cr3 + gMmuVOffset), (virtaddr)vaddr + (p*PAGE_SIZE), (physaddr)block + (p*PAGE_SIZE),  MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE);
            }
            void* kdest = (void*)((uint64_t)block + gMmuVOffset + offset);
            printf("ldr: kernel dest to copy to 0x%lx (in new cr3 this is @ 0x%lx) (kdest calculated by adding 0x%lx + 0x%lx + 0x%lx)\r\n", kdest, current->p_vaddr, block, gMmuVOffset, offset);
            memcpy((void*)kdest, (void*)((uint64_t)Elf + current->p_offset), filesz);
            if (memsz > filesz) memset((void*)((uint64_t)kdest + filesz), 0, memsz - filesz);
        }
    }
    KernelInformation* kinfo = KernelGetInformation();
    void (*entry)() = (void(*)())Elf->e_entry;
    printf("ldr: elf64: create new thread with entry 0x%lx relative to page table.\r\n", entry);
    ThreadCtrlBlk* thr = ThreadNew(entry);
    ProcAttachThread(proc, thr);
    ThreadAdd(thr);
    proc->Next = kinfo->ProcessListHead;
    kinfo->ProcessListHead = proc;
    kinfo->CurrentProcess = proc;
    return KSUCCESS;
}
