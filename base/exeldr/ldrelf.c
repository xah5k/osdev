#include "ldrelf.h"
#include <external/elf.h>
#include <memory.h>
#include <external/printf.h>
#include <mm/heap.h>
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
    for (int i = 0; i < Elf->e_phnum; i++) {
        Elf64_Phdr* current = &PHdr[i];
        if (current->p_type == PT_LOAD) {
            uint64_t filesz = current->p_filesz;
            uint64_t memsz = current->p_memsz;
        }
    }
    return KSUCCESS;
}
