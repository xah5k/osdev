#pragma once
// todo: better name for header

#include <stdint.h>
#include <stddef.h>
#include <kernel.h>
#include <util/util.h>

// https://uefi.org/specs/UEFI/2.10/05_GUID_Partition_Table_Format.html#gpt-header
typedef struct {
    char Signature[8];
    uint32_t Revision;
    uint32_t HeaderSz;
    uint32_t HeaderCrc;
    uint32_t Rsv0;
    uint64_t MyLba;
    uint64_t AltLba;
    uint64_t FirstUsableLba;
    uint64_t LastUsableLba;
    uint8_t DiskGuid[16];
    uint64_t PartitionEntryLba;
    uint32_t PartitionNumber;
    uint32_t PartitionEntrySz;
    uint32_t PartitionEntryArrCrc;
    // entire rest of the block is reserved
} __attribute__((packed)) GptHdr;

// https://uefi.org/specs/UEFI/2.10/05_GUID_Partition_Table_Format.html#gpt-partition-entry-array
typedef struct {
    uint8_t PartitionTypeGuid[16];
    uint8_t UniquePartitionGuid[16];
    uint64_t StartLba;
    uint64_t EndLba;
    uint64_t Attributes;
    wchar_t PartitionName[36];
    // rest reserved
} __attribute__((packed)) GptPartEnt;

KSTATUS PtableEnumerate(void* Lba1);