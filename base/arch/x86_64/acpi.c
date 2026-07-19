#include "acpi.h"
#include <arch/x86_64/cpu/paging.h>
#include <memory.h>
#include <printfwrapper.h>
#include <kedriver.h>

void* AcpiFindTable(AcpiRsdtTable* rsdt, char* signature) {
    int entries = ((rsdt->header.Length - sizeof(rsdt->header)) / 4);
    //printf("acpi: xsdt=0x%lx\r\n", xsdt);
    for (int i = 0; i < entries; i++) {
       // printf("acpi: before creating header\r\n");
        AcpiTableHeader* header = (AcpiTableHeader*)(rsdt->Tables[i] + gMmuVOffset);
        //printf("acpi: i=%d header@0x%lx total entries=%d\r\n", i, header, entries);
        if (memcmp(header->Signature, signature, 4) == 0) {
            return (void*)header;
        }
    }
    return NULL;
}
KE_EXPORT_SYMBOL(AcpiFindTable);