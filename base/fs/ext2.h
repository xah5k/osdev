#pragma once
#include <fs/vfs.h>
#include <stdint.h>

typedef enum {
    EXT2_OS_LINUX,
    EXT2_OS_HURD,
    EXT2_OS_MASIX,
    EXT2_OS_FREEBSD,
    EXT2_OS_LITES
} Ext2SbCreatorOs;

#define EXT2_GOOD_OLD_REV 0
#define EXT2_DYNAMIC_REV 1

typedef struct {
    uint32_t InodeCount;
    uint32_t BlockCount;
    uint32_t RsvBlockCount;
    uint32_t FreeBlocksCount;
    uint32_t FreeInodeCount;
    uint32_t FirstDataBlk;
    uint32_t LogBlkSz;
    uint32_t LogFragSz;
    uint32_t BlksPerGroup;
    uint32_t FragsPerGroup;
    uint32_t InodesPerGroup;
    uint32_t LastMountTime;
    uint32_t LastWriteTime;
    uint16_t MountCount;
    uint16_t MaxMountCount;
    uint16_t Magic;
    uint16_t State;
    uint16_t Errors;
    uint16_t MinorRevision;
    uint32_t LastCheck;
    uint32_t CheckInterval;
    uint32_t CreatorOs;
    uint32_t Revision;
    uint32_t RsvUid; // should js be 0
    uint32_t RsvGid; // same applies here
} Ext2Superblock;

// if revision == EXT2_DYNAMIC_REV
typedef struct {
    uint32_t FirstIno;
    uint16_t InodeSz;
    uint16_t BlkGroupNumber;
    uint32_t FeatureCompat;
    uint32_t FeatureIncompat;
    uint32_t FeatureRoCompat;
    uint8_t Uuid[16];
    uint8_t VolName[16];
    char LastMounted[16];
    uint32_t AlgoBitmap;
    uint8_t PreallocBlks;
    uint8_t PreallocDirBlks;
    uint16_t RsvAlign;
    uint8_t JournalUuid[16];
    uint32_t JournalInode;
    uint32_t JournalDev;
    uint32_t LastOrphan;
    uint32_t HashSeed[4];
    uint8_t DefHashVersion;
    uint8_t RsvPad[3];
    uint32_t DefaultMountOption;
    uint32_t FirstMetaBg;
} Ext2SbDynRev;

void Ext2SbDump(void* base);