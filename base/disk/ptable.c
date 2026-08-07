#include <disk/ptable.h>
#include <memory.h>
#include <printfwrapper.h>
#include <kernel.h>
#include <disk/ahci.h>
#include <mm/pmm.h>
#include <fs/ext2.h>

const uint8_t gZeroGuid[16] = {0};
static int gInitPart1 = 0;
KSTATUS PtableEnumerate(void* Lba1) {
    GptHdr* Hdr = (GptHdr*)Lba1;

    if (memcmp((void*)Hdr->Signature, (void*)"EFI PART", 8) != 0) {
        printf("ptable: invalid signature for header.\r\n");
        return KINVALID;
    }
    printf("ptable: guid part table at lba %d\r\n", Hdr->MyLba);
    printf("ptable: guid part table alt lba %d\r\n", Hdr->AltLba);
    printf("ptable: guid part table part entry lba %d\r\n", Hdr->PartitionEntryLba);
    printf("ptable: guid part table part entry size %d\r\n", Hdr->PartitionEntrySz);
    uint32_t PartEntrySz = Hdr->PartitionEntrySz;
    uint32_t ByteCount = (Hdr->PartitionNumber * PartEntrySz);

    uint8_t* buffer = PmmAllocatePages(ByteCount / MMU_PAGE_SIZE);
    uint8_t* vbuf = (uint8_t*)P2V(buffer);
    memset(vbuf, 0, ByteCount);
    uint32_t SectorsCount = (ByteCount + 511) / 512;

    KSTATUS r = AhciPortRead(AhciGetPort(0), Hdr->PartitionEntryLba, SectorsCount, buffer);
    if (r != KSUCCESS) {
        PmmFree(buffer);
        printf("ptable: fail to read from ahci.\r\n");
        return r;
    }
    uint8_t* base = vbuf;
    for (uint32_t i = 0; i < Hdr->PartitionNumber; i++) {
        GptPartEnt* Entry = (GptPartEnt*)base;
        if (memcmp((void*)Entry->PartitionTypeGuid, gZeroGuid, 16) == 0) continue;
        printf("ptable: partition %d: name=", i);
        UtilPrintW(Entry->PartitionName, 36);
        printf("\r\nptable: partition %d: start lba = %d end lba = %d\r\n", i, Entry->StartLba, Entry->EndLba);
        if (i == 1 && gInitPart1 == 0) {
            Ext2SbInit(Entry->StartLba);
            gInitPart1 = 1;
        }
        base += PartEntrySz;
    }
    return KSUCCESS;
}