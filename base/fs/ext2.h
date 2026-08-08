#pragma once
#include <fs/vfs.h>
#include <stdint.h>
#include <disk/ahci.h>

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
    uint16_t RsvUid; // should js be 0
    uint16_t RsvGid; // same applies here
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
    uint32_t HashSeed1;
    uint32_t HashSeed2;
    uint32_t HashSeed3;
    uint32_t HashSeed4;
    uint8_t DefHashVersion;
    uint8_t RsvPad[3];
    uint32_t DefaultMountOption;
    uint32_t FirstMetaBg;
    uint8_t Rsv[760];
} Ext2SbDynRev;

typedef struct {
    uint32_t BlockBitmap;
    uint32_t InodeBitmap;
    uint32_t InodeTable;
    uint16_t FreeBlocksCount;
    uint16_t FreeInodesCount;
    uint16_t UsedDirsCount;
    uint16_t RsvPad;
    uint8_t Rsv[12];
} Ext2BlkGroupDesc;

typedef struct {
    uint16_t TypePerm;
    uint16_t Uid;
    uint32_t SizeLow;
    uint32_t LastAccess;
    uint32_t CreationTime;
    uint32_t LastModification;
    uint32_t DeletionTime;
    uint16_t Gid;
    uint16_t HardlinkCount;
    uint32_t SectorsInUse;
    uint32_t Flags;
    uint32_t OsSpecific;
    uint32_t Dbp0;
    uint32_t Dbp1;
    uint32_t Dbp2;
    uint32_t Dbp3;
    uint32_t Dbp4;
    uint32_t Dbp5;
    uint32_t Dbp6;
    uint32_t Dbp7;
    uint32_t Dbp8;
    uint32_t Dbp9;
    uint32_t Dbp10;
    uint32_t Dbp11;
    uint32_t Sibp;
    uint32_t Dibp;
    uint32_t Tibp;
    uint32_t GenNum;
    uint32_t ExtendedAcl;
    uint32_t SizeHigh;
    uint32_t BlkAddrFrag;
    uint8_t OsRsv[12];
} Ext2InoData;

typedef struct {
    uint32_t Inode;
    uint16_t Reclen;
    uint8_t NameLen;
    uint8_t FileType;
    char Name[];
} Ext2Dirent;

#define EXT2_TYPE_FIFO 0x1000
#define EXT2_TYPE_CHARDEV 0x2000
#define EXT2_TYPE_DIR 0x4000
#define EXT2_TYPE_BLKDEV 0x6000
#define EXT2_TYPE_FILE 0x8000
#define EXT2_TYPE_SYMLINK 0xA000
#define EXT2_TYPE_USOCKET 0xC000

#define EXT2_PERM_OEXEC 0x001
#define EXT2_PERM_OWRITE 0x002
#define EXT2_PERM_OREAD 0x004
#define EXT2_PERM_GEXEC 0x008
#define EXT2_PERM_GWRITE 0x010
#define EXT2_PERM_GREAD 0x020
#define EXT2_PERM_UEXEC 0x040
#define EXT2_PERM_UWRITE 0x080
#define EXT2_PERM_UREAD 0x100

#define EXT2_ROOT_INODE 2
// internal structure to avoid duplication
typedef struct {
    KeAhciPort* Port;
    uint64_t PartitionStartLba;
    uint32_t BlockSize;
    uint32_t InodesPerGroup;
    uint32_t BlocksPerGroup;
    uint32_t InodeSz;
    uint32_t InodeCount;
    uint32_t BlockCount;
    uint32_t FirstDataBlk;
    uint32_t GroupsCount;
    void* Bgdtvbuf; // ts is the one we should use if actually reading/writing to it at all times
    void* Bgdtpbuf; // for freeing
    Ext2Superblock* SbPtr;
    VfsDrive* Ext2Drive; // the actual vfs drive
    VfsFile* Ext2Files;
} KeExt2Volume;
void Ext2SbInit(uint64_t lba, uint64_t partnum);