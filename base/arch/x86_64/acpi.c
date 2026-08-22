#include "acpi.h"
#include <arch/x86_64/cpu/paging.h>
#include <memory.h>
#include <printfwrapper.h>
#include <kedriver.h>
#include <uacpi/sleep.h>
#include <uacpi/utilities.h>
#include <uacpi/resources.h>
#include "pci/pci.h"
#include <mm/heap.h>
static KePciGsiResolv* gGsiResHead = NULL;
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

KSTATUS AcpiSystemShutdown() {
    uacpi_status r = uacpi_enter_sleep_state_simple(UACPI_SLEEP_STATE_S5);
    if (uacpi_unlikely_error(r)) {
        printf("acpi: failed to enter S5 sleep (shutdown). error message: \"%s\"\r\n", uacpi_status_to_string(r));
        return KFAIL;
    }
    return KSUCCESS; // unreachable
}

static uacpi_iteration_decision AcpiRsrcLoop(void* user, uacpi_resource* resource) {
    uint64_t gsi = UINT64_MAX;
    if (resource->type == UACPI_RESOURCE_TYPE_IRQ) {
        // legacy
        gsi = resource->irq.irqs[0];
        if (user) *(uint64_t*)user = gsi;
        return UACPI_ITERATION_DECISION_BREAK;
    } else if (resource->type == UACPI_RESOURCE_TYPE_EXTENDED_IRQ) {
        gsi = resource->extended_irq.irqs[0];
        if (user) *(uint64_t*)user = gsi;
        return UACPI_ITERATION_DECISION_BREAK;
    }
    return UACPI_ITERATION_DECISION_CONTINUE;
} 

KSTATUS AcpiResolvePciGsi() {
    // todo actually properly match ids but like
    // that only matters once i try on real hardware
    uacpi_namespace_node *root_bridge_node;
    uacpi_status ret = uacpi_namespace_node_find(
        uacpi_namespace_get_predefined(UACPI_PREDEFINED_NAMESPACE_ROOT),
        "_SB_.PCI0",
        &root_bridge_node
    );
    if (uacpi_unlikely_error(ret)) {
        return KFAIL;
    }
    uacpi_pci_routing_table *prt;
    ret = uacpi_get_pci_routing_table(root_bridge_node, &prt);
    if (uacpi_unlikely_error(ret)) {
        printf("acpi: fail: failed to get PRT. err \"%s\" \r\n", uacpi_status_to_string(ret));
        return KFAIL;
    }
    uacpi_namespace_node *sb_node = uacpi_namespace_get_predefined(UACPI_PREDEFINED_NAMESPACE_SB);
    // js loop through the table
    for (uacpi_size i = 0; i < prt->num_entries; i++) {
        uacpi_pci_routing_table_entry* e = &prt->entries[i];
        uint16_t Func = (e->address & 0xFFFF);
        uint32_t Device = (e->address >> 16) & 0xFFFF;
        uacpi_namespace_node* link_node = e->source;
        uacpi_resources *resources;
        uint64_t gsi;
        if (e->source == UACPI_NULL) {
            gsi = e->index;    
        } else {
            ret = uacpi_get_current_resources(link_node, &resources);
            if (uacpi_unlikely_error(ret)) {
                printf("acpi: warn: failed to get resources list. error message: \"%s\"\r\n", uacpi_status_to_string(ret));
                continue;
            }
            ret = uacpi_for_each_resource(resources, AcpiRsrcLoop, &gsi);
            uacpi_free_resources(resources);
            if (uacpi_unlikely_error(ret)) {
                printf("acpi: warn: failed to interate through resources list. error message: \"%s\"\r\n", uacpi_status_to_string(ret));
                continue;
            }
            if (gsi == UINT64_MAX) {
                printf("acpi: warn: gsi was populated as UINT64_MAX!! (AcpiRsrcLoop didn't find IRQ number!)\r\n");
                continue; // continue here cuz it might js be the device doesn't have have an actual IRQ
            }
        }
        KePciGsiResolv* Resolv = MmAllocate(sizeof(KePciGsiResolv));
        Resolv->Bus = 0; // todo
        Resolv->Dev = Device;
        Resolv->Func = Func;
        Resolv->Pin = e->pin;
        Resolv->Gsi = gsi;
        Resolv->Next = gGsiResHead;
        gGsiResHead = Resolv;
    }
    uacpi_free_pci_routing_table(prt);
    return KSUCCESS;
}

uint32_t AcpiPciGsiLookup(uint64_t Bus, uint64_t Dev, uint8_t Pin) {
    KePciGsiResolv* Current = gGsiResHead;
    while (Current != NULL) {
        if (Current->Bus == Bus && Current->Dev == Dev && Current->Pin == (Pin - 1)) {
            return (uint32_t)Current->Gsi;
        }
        Current = Current->Next;
    }
    return (uint32_t)-1;
}
KE_EXPORT_SYMBOL(AcpiPciGsiLookup);