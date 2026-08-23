#include <uacpi/kernel_api.h>
#include <kernel.h>
#include "cpu/paging.h"
#include "cpu/lapic.h"
#include "cpu/idt.h"
#include <printfwrapper.h>
#include "ports.h"
#include <mm/heap.h>
#include <sched/process.h>
#include <util/spinlock.h>
#include <stdatomic.h>
#include <memory.h>
#include <uacpi/uacpi.h>
#include <uacpi/utilities.h>
#include <uacpi/event.h>
#include <mm/pmm.h>
#include <util/util.h>
#include <hal/pci.h>
// basically implements (or stubs) things that uACPI needs
extern uint64_t gCpuLapicTicksPerMs;
extern uint64_t gCpuLapicTicksPer10ms;
extern uint64_t PmmTotalFreePhysRam;

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out_rsdp_address) {
    KATTEMPT(KernelGetInformation()->rsdp);
    *out_rsdp_address = (uacpi_phys_addr)V2P(KernelGetInformation()->rsdp);
    return UACPI_STATUS_OK;
}

void *uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len) {
    uacpi_phys_addr aligned = MMU_ROUND_PAGE_DOWN(addr);
    uacpi_size end = MMU_ROUND_PAGE_UP(addr + len);
    uacpi_size roundedlen = end - aligned;
    for (uacpi_size i = 0; i < roundedlen; i+=MMU_PAGE_SIZE) {
        MmuMapPage((pagetable*)P2V(_x86_64_get_pml4()), P2V(aligned + i), aligned + i, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE);
    }
    return (void*)(P2V(aligned) + (addr - aligned));
}

void uacpi_kernel_unmap(void *addr, uacpi_size len) {
    // nop cuz like WE love wasting memory
}

#ifndef UACPI_FORMATTED_LOGGING
void uacpi_kernel_log(uacpi_log_level level, const uacpi_char* string) {
    switch (level) {
        case UACPI_LOG_ERROR: {
            printf("uacpi: ERROR: %s", string);
            break;
        }
        case UACPI_LOG_WARN: {
            printf("uacpi: WARN: %s", string);
            break;
        }
        case UACPI_LOG_INFO: {
            printf("uacpi: INFO: %s", string);
            break;
        }
        case UACPI_LOG_TRACE: {
            printf("uacpi: TRACE: %s", string);
            break;
        }
        case UACPI_LOG_DEBUG: {
            printf("uacpi: DEBUG: %s", string);
            break;
        }
    }
}
#endif

uacpi_status uacpi_kernel_pci_device_open(
    uacpi_pci_address address, uacpi_handle *out_handle
) {
    KePciDeviceHdr* Current = PciGetLinkedList();
    if (!Current) return UACPI_STATUS_NOT_FOUND;
    while (Current != NULL) {
        if (address.bus == Current->Bus && address.device == Current->Dev && address.function == Current->Func) {
            *out_handle = Current;
            return UACPI_STATUS_OK;
        }
        Current = Current->Next;
    }
    return UACPI_STATUS_NOT_FOUND;
}

void uacpi_kernel_pci_device_close(uacpi_handle) {
    // ...
}


uacpi_status uacpi_kernel_pci_read8(
    uacpi_handle device, uacpi_size offset, uacpi_u8 *value
) {
    KePciDeviceHdr* Device = (KePciDeviceHdr*)device;
    if (!Device) {
        return UACPI_STATUS_INVALID_ARGUMENT;
    }
    uint32_t Value = PciReadDword(Device->EcamBase, Device->Bus, Device->Dev, Device->Func, (uint8_t)offset);
    *value = (uacpi_u8)Value;
    return UACPI_STATUS_OK;
}


uacpi_status uacpi_kernel_pci_read16(
    uacpi_handle device, uacpi_size offset, uacpi_u16 *value
) {
    KePciDeviceHdr* Device = (KePciDeviceHdr*)device;
    if (!Device) {
        return UACPI_STATUS_INVALID_ARGUMENT;
    }
    uint32_t Value = PciReadDword(Device->EcamBase, Device->Bus, Device->Dev, Device->Func, (uint8_t)offset);
    *value = (uacpi_u16)Value;
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read32(
    uacpi_handle device, uacpi_size offset, uacpi_u32 *value
) {
    KePciDeviceHdr* Device = (KePciDeviceHdr*)device;
    if (!Device) {
        return UACPI_STATUS_INVALID_ARGUMENT;
    }
    uint32_t Value = PciReadDword(Device->EcamBase, Device->Bus, Device->Dev, Device->Func, (uint8_t)offset);
    *value = Value;
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write8(
    uacpi_handle device, uacpi_size offset, uacpi_u8 value
) {
    KePciDeviceHdr* Device = (KePciDeviceHdr*)device;
    if (!Device) {
        return UACPI_STATUS_INVALID_ARGUMENT;
    }
    PciWriteDword(Device->EcamBase, Device->Bus, Device->Dev, Device->Func, (uint8_t)offset, (uint32_t)value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write16(
    uacpi_handle device, uacpi_size offset, uacpi_u16 value
) {
    KePciDeviceHdr* Device = (KePciDeviceHdr*)device;
    if (!Device) {
        return UACPI_STATUS_INVALID_ARGUMENT;
    }
    PciWriteDword(Device->EcamBase, Device->Bus, Device->Dev, Device->Func, (uint8_t)offset, (uint32_t)value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write32(
    uacpi_handle device, uacpi_size offset, uacpi_u32 value
) {
    KePciDeviceHdr* Device = (KePciDeviceHdr*)device;
    if (!Device) {
        return UACPI_STATUS_INVALID_ARGUMENT;
    }
    PciWriteDword(Device->EcamBase, Device->Bus, Device->Dev, Device->Func, (uint8_t)offset, value);
    return UACPI_STATUS_OK;
}

// we're on x86-64 so js do nothing
uacpi_status uacpi_kernel_io_map(
    uacpi_io_addr base, uacpi_size len, uacpi_handle *out_handle
) {
    *(uacpi_io_addr*)out_handle = base;
    return UACPI_STATUS_OK;
}

void uacpi_kernel_io_unmap(uacpi_handle handle) {
    (void)handle;
}

uacpi_status uacpi_kernel_io_read8(
    uacpi_handle handle, uacpi_size offset, uacpi_u8 *out_value
) {
    uacpi_io_addr base = (uacpi_io_addr)handle + offset;
    *out_value = inb(base);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read16(
    uacpi_handle handle, uacpi_size offset, uacpi_u16 *out_value
) {
    uacpi_io_addr base = (uacpi_io_addr)handle + offset;
    *out_value = inw(base);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read32(
    uacpi_handle handle, uacpi_size offset, uacpi_u32 *out_value
) {
    uacpi_io_addr base = (uacpi_io_addr)handle + offset;
    *out_value = inl(base);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write8(
    uacpi_handle handle, uacpi_size offset, uacpi_u8 in_value
) {
    uacpi_io_addr base = (uacpi_io_addr)handle + offset;
    outb(base, in_value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write16(
    uacpi_handle handle, uacpi_size offset, uacpi_u16 in_value
) {
    uacpi_io_addr base = (uacpi_io_addr)handle + offset;
    outw(base, in_value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write32(
    uacpi_handle handle, uacpi_size offset, uacpi_u32 in_value
) {
    uacpi_io_addr base = (uacpi_io_addr)handle + offset;
    outl(base, in_value);
    return UACPI_STATUS_OK;
}

void *uacpi_kernel_alloc(uacpi_size size) {
    void* r = MmAllocate(size);
    return r;
}

void uacpi_kernel_free(void *mem) {
    if (!mem) return;
    MmFree(mem);
}

uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void) {
    uint64_t ms = CpuLapticTimerGetTick() / gCpuLapicTicksPerMs;
    return ms * 1000000;
}

void uacpi_kernel_stall(uacpi_u8 usec) {
    uint64_t start = ((CpuLapticTimerGetTick() / gCpuLapicTicksPerMs)*1000);
    while (((CpuLapticTimerGetTick() / gCpuLapicTicksPerMs)*1000) < start + usec);
}


void uacpi_kernel_sleep(uacpi_u64 msec) {
    uint64_t start = ( CpuLapticTimerGetTick() / gCpuLapicTicksPerMs);
    while (( CpuLapticTimerGetTick() / gCpuLapicTicksPerMs) < start + msec);
}

uacpi_interrupt_state uacpi_kernel_disable_interrupts(void) {
    uint64_t rfl;
    asm volatile ("pushfq; pop %0" : "=r"(rfl));
    asm volatile ("cli");
    return rfl;
}

void uacpi_kernel_restore_interrupts(uacpi_interrupt_state state) {
    asm volatile("push %0; popfq" : : "r"(state));
}

uacpi_thread_id uacpi_kernel_get_thread_id(void) {
    return (uacpi_thread_id)ThrGetCurrent()->tid; // todo: tids might be the same for different threads. (like tid 0 in pid1 has the same tid as tid0 in pid2)
}

uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request* request) {
    switch (request->type) {
        case UACPI_FIRMWARE_REQUEST_TYPE_BREAKPOINT: {
            break;
        }
        case UACPI_FIRMWARE_REQUEST_TYPE_FATAL: {
            printf("uacpi: firmware fatal!\r\n");
            printf("uacpi: type=%d code=%x arg=%lx\r\n", request->fatal.type, request->fatal.code, request->fatal.arg);
            KdBugcheck(KERNEL_ACPI_FIRMWARE_FATAL, NULL);
            // should never return
        }
        default: {
            break;
        }
    }
    return UACPI_STATUS_OK;
}
extern irqhandler handlers[256]; // 256 handlers
extern void* handlerargs[256]; // this is such a stupid way of passing args
uacpi_interrupt_handler uacpi_handlers[256]; // yes yes i know

void uacpi_int_handler(CpuInterruptArgs* registers) {
    if (uacpi_handlers[registers->intnum]) {
        uacpi_handlers[registers->intnum](handlerargs[registers->intnum]);
    }
}

uacpi_status uacpi_kernel_install_interrupt_handler(
    uacpi_u32 irq, uacpi_interrupt_handler handler, uacpi_handle ctx,
    uacpi_handle *out_irq_handle
) {
    CpuRegisterHandler(irq, (irqhandler)uacpi_int_handler);
    uacpi_handlers[irq] = handler;
    CpuRegisterHandlerArg(irq, ctx);
    *(uacpi_u32*)out_irq_handle = irq; 
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_uninstall_interrupt_handler(
    uacpi_interrupt_handler, uacpi_handle irq_handle
) {
    uacpi_kernel_log(UACPI_LOG_WARN, "remove interrupt handler todo\r\n");
    return UACPI_STATUS_OK;
}

uacpi_handle uacpi_kernel_create_spinlock(void) {
    Spinlock* lock = MmAllocate(sizeof(Spinlock));
    memset(lock, 0, sizeof(Spinlock));
    atomic_flag_clear(&lock->x);
    return (uacpi_handle)lock;
}

void uacpi_kernel_free_spinlock(uacpi_handle lock) {
    MmFree(lock);
}

uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle lock) {
    return SpnLckAcquireRfl(lock);
}

void uacpi_kernel_unlock_spinlock(uacpi_handle lock, uacpi_cpu_flags rfl) {
    SpnLckReleaseRfl(lock, rfl);
}

uacpi_status uacpi_kernel_schedule_work(
    uacpi_work_type, uacpi_work_handler, uacpi_handle ctx
) {
    // todo
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_wait_for_work_completion(void) {
    // todo
    return UACPI_STATUS_OK;
}

// cuz i dont think it should be much difference between mutexes and spinlocks
uacpi_handle uacpi_kernel_create_mutex(void) {
    return uacpi_kernel_create_spinlock();
}
void uacpi_kernel_free_mutex(uacpi_handle lock) {
    uacpi_kernel_free_spinlock(lock);
}

uacpi_status uacpi_kernel_acquire_mutex(uacpi_handle lock, uacpi_u16 timeout) {
    if (timeout == 0 || timeout == 0xFFFF) {
        SpnLckAcquire(lock);
        return UACPI_STATUS_OK;
    }
    while (atomic_flag_test_and_set_explicit(&((Spinlock*)lock)->x, memory_order_acquire)) {
        uint64_t start = CpuLapticTimerGetTick();
        if (CpuLapticTimerGetTick() > (start + timeout)) {
            return UACPI_STATUS_TIMEOUT;
        }
    }
    return UACPI_STATUS_OK;
}

void uacpi_kernel_release_mutex(uacpi_handle lock) {
    SpnLckRelease(lock);
}

uacpi_bool uacpi_kernel_wait_for_event(uacpi_handle, uacpi_u16) {
    return UACPI_FALSE; // we dont have semaphores yet
}

void uacpi_kernel_signal_event(uacpi_handle) {
    // todo
}

void uacpi_kernel_reset_event(uacpi_handle) {
    // todo
}

uacpi_handle uacpi_kernel_create_event(void) {
    void* placeholder = MmAllocate(1024);
    return placeholder;
}
void uacpi_kernel_free_event(uacpi_handle placeholder) {
    MmFree(placeholder);
}

KSTATUS uAcpiInitalize() {
    uacpi_status r = uacpi_initialize(0);
    if (uacpi_unlikely_error(r)) {
        printf("acpi: initalizing uacpi failed. error string: \"%s\"\r\n", uacpi_status_to_string(r));
        return KFAIL;    
    }
    r = uacpi_namespace_load();
    if (uacpi_unlikely_error(r)) {
        printf("acpi: load uacpi namespace failed. error string: \"%s\"\r\n", uacpi_status_to_string(r));
        return KFAIL;    
    }
    r = uacpi_namespace_initialize();
    if (uacpi_unlikely_error(r)) {
        printf("acpi: initalize uacpi namespace failed. error string: \"%s\"\r\n", uacpi_status_to_string(r));
        return KFAIL;
    }
    uacpi_set_interrupt_model(UACPI_INTERRUPT_MODEL_IOAPIC);
    r = uacpi_finalize_gpe_initialization();
    if (uacpi_unlikely_error(r)) {
        printf("acpi: uacpi finalize gpe init failed. error string: \"%s\"\r\n", uacpi_status_to_string(r));
        return KFAIL;
    }
    printf("acpi: initalized uacpi successfully.\r\n");
    return KSUCCESS;
}