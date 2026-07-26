#include "ldrelf.h"
#include <external/elf.h>
#include <memory.h>
#include <external/printf.h>
#include <mm/pmm.h>
#include <mm/heap.h>
#include <sched/process.h>
#include <kedriver.h>
#include <util/util.h>
KSTATUS LdrElfExecute(void* addr, uint8_t priv, uint64_t* pidout, const char** argv, int argc, const char* name) {
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

    ProcessCtrlBlk* proc = ProcessNew(name);
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
                MmuMapPage((pagetable*)((uint64_t)proc->cr3 + gMmuVOffset), (virtaddr)vaddr + (p*PAGE_SIZE), (physaddr)block + (p*PAGE_SIZE),  MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE | MMU_PAGE_BIT_US_USER);
            }
            void* kdest = (void*)((uint64_t)block + gMmuVOffset + offset);
            // printf("ldr: kernel dest to copy to 0x%lx (in new cr3 this is @ 0x%lx) (kdest calculated by adding 0x%lx + 0x%lx + 0x%lx)\r\n", kdest, current->p_vaddr, block, gMmuVOffset, offset);
            memcpy((void*)kdest, (void*)((uint64_t)Elf + current->p_offset), filesz);
            if (memsz > filesz) memset((void*)((uint64_t)kdest + filesz), 0, memsz - filesz);
        }
    }
    KernelInformation* kinfo = KernelGetInformation();
    uint64_t entry = (uint64_t)Elf->e_entry;
    // printf("ldr: elf64: create new thread with entry 0x%lx relative to page table.\r\n", entry)
    printf("argv=0x%lx argc=(literal) %d\r\n", argv, argc);
    ThreadCtrlBlk* thr = ThreadNew((void*)entry, priv, argv, argc);
    ProcAttachThread(proc, thr);
    if (priv > SCHED_PRIV_KERNEL) {
        ThreadMapUserStack(thr);
    }
    ThreadAdd(thr);
    proc->Parent = ThrGetCurrent()->ParentProc;
    proc->Next = kinfo->ProcessListHead;
    kinfo->ProcessListHead = proc;
    kinfo->CurrentProcess = proc;
    if (pidout != NULL) *pidout = proc->pid;
    return KSUCCESS;
}

typedef struct {
    void* Base;
} LdrSectionLoadInf;
KSTATUS LdrElfDriverExec(void* addr, KeDriverObj** driver) {
    Elf64_Ehdr* Elf = (Elf64_Ehdr*)addr;
    if (memcmp(Elf->e_ident, ELFMAG, 4) != 0) {
        printf("ldr: elf64: invalid magic\r\n");
        return KINVALID;
    }
    if ((Elf->e_ident[EI_CLASS] != ELFCLASS64) && (Elf->e_machine != EM_X86_64)) {
        printf("ldr: elf64: invalid cpu architecture\r\n");
        return KINVALID;
    }
    if (Elf->e_type != ET_REL) {
        printf("ldr: elf64: driver not compiled as ET_REL!\r\n");
        return KINVALID;
    }
    Elf64_Shdr* SHdrs = (Elf64_Shdr*)((uint8_t*)addr + Elf->e_shoff);
    uint16_t SHnum = Elf->e_shnum;  

    LdrSectionLoadInf* LoadInfo = MmAllocate(sizeof(LdrSectionLoadInf) * SHnum);
    if (!LoadInfo) return KOOMERR;
    memset(LoadInfo, 0, sizeof(LdrSectionLoadInf) * SHnum);

    for (int i = 0; i < SHnum; i++) {
        Elf64_Shdr* current = &SHdrs[i];
        if (!(current->sh_flags & SHF_ALLOC) || current->sh_size == 0) continue;
        void* Buf = MmAllocate(current->sh_size);
        if (!Buf) {
            printf("ldr: elf64: allocation for section %d failed.\r\n", i);
            MmFree(LoadInfo);
            return KOOMERR;
        }
        if (current->sh_type == SHT_NOBITS) {
            memset(Buf, 0, current->sh_size);
        }  else {
            memcpy(Buf, (uint8_t*)addr + current->sh_offset, current->sh_size);
        }
        LoadInfo[i].Base = Buf;
    }
    Elf64_Shdr* SymbolTable = NULL;
    for (int i = 0; i < SHnum; i++) {
        if (SHdrs[i].sh_type == SHT_SYMTAB) {
            SymbolTable = &SHdrs[i];
            break;
        }
    }
    if (!SymbolTable) {
        printf("ldr: elf64: no symbol table found!\r\n");
        MmFree(LoadInfo);
        return KOOMERR;
    }
    Elf64_Sym* Symbols = (Elf64_Sym*)((uint8_t*)addr + SymbolTable->sh_offset);
    int SymNum = SymbolTable->sh_size / sizeof(Elf64_Sym);
    char* StrSymTable = (char*)addr + SHdrs[SymbolTable->sh_link].sh_offset;

    uint64_t* Resolved = MmAllocate(sizeof(uint64_t) * SymNum);
    for (int i = 0; i < SymNum; i++) {
        Elf64_Sym* Sym = &Symbols[i];
        if (Sym->st_shndx == SHN_UNDEF) {
            if (Sym->st_name == 0) {
                Resolved[i] = 0;
                continue;
            }
            const char* Name = StrSymTable + Sym->st_name;
            void* Address = KeGetExport(Name);
            if (!Address) {
                printf("ldr: elf64: failed to resolve symbol '%s'\r\n", Name);
                MmFree(LoadInfo);
                MmFree(Resolved);
                return KINVALID;
            }
            Resolved[i] = (uint64_t)Address;
        } else {
            Resolved[i] = (uint64_t)LoadInfo[Sym->st_shndx].Base + Sym->st_value;
        }
    }

    for (int i = 0; i < SHnum; i++) {
        Elf64_Shdr* current = &SHdrs[i];
        if (current->sh_type != SHT_RELA) continue;

        int TargetSecIdx = current->sh_info;
        void* TargetBase = LoadInfo[TargetSecIdx].Base;
        if (!TargetBase) continue;

        Elf64_Rela* Relas = (Elf64_Rela*)((uint8_t*)addr + current->sh_offset);
        int RelasNum = current->sh_size / sizeof(Elf64_Rela);

        for (int r = 0; r < RelasNum; r++) {
            Elf64_Rela* rel = &Relas[r];
            uint32_t SymIdx = ELF64_R_SYM(rel->r_info);
            uint32_t Type = ELF64_R_TYPE(rel->r_info);
            uint64_t SymAddr = Resolved[SymIdx];
            void* PatchAddr = (uint8_t*)TargetBase + rel->r_offset;
            switch (Type) {
                case R_X86_64_64: {
                    uint64_t Value = SymAddr + rel->r_addend;
                    *(uint64_t*)PatchAddr = Value;
                    break;
                }
                case R_X86_64_PC32:
                case R_X86_64_PLT32: {
                    int64_t Value = (int64_t)(SymAddr + rel->r_addend) - (int64_t)PatchAddr;
                    *(int32_t*)PatchAddr = (int32_t)Value;
                    break;
                }
                case R_X86_64_32:
                case R_X86_64_32S: {
                    uint64_t Value = SymAddr + rel->r_addend;
                    *(uint32_t*)PatchAddr = (uint32_t)Value;
                    break;
                }
                default: {
                    printf("ldr: elf64: unsupported relocation type %d\r\n", Type);
                    MmFree(LoadInfo);
                    MmFree(Resolved);
                    return KINVALID;
                }
            }
        }
    }
    void* EntryAddress = NULL;
    for (int i = 0; i < SymNum; i++) {
        const char* Name = StrSymTable + Symbols[i].st_name;
        if (strcmp(Name, "DriverEntry") == 0) {
            EntryAddress = (void*)Resolved[i];
            break;
        }
    }
    if (!EntryAddress) {
        printf("ldr: elf64: no driver entry symbol found!\r\n");
        MmFree(LoadInfo);
        MmFree(Resolved);
        return KINVALID;
    }
    KeDriverObj* Driver = MmAllocate(sizeof(KeDriverObj));
    memset(Driver, 0, sizeof(KeDriverObj));
    Driver->Initalize = (KSTATUS(*)(KeDriverObj*))EntryAddress;
    *driver = Driver;
    return KSUCCESS;
}
