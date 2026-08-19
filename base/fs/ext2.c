#include <fs/ext2.h>
#include <printfwrapper.h>
#include <disk/ahci.h>
#include <mm/pmm.h>
#include <mm/heap.h>
#include <memory.h>
#include <util/util.h>

static uint64_t gCurrFileIdx = 0; // used by ext2createvfstable to keep track of where its at
static KeExt2Volume* gVolume; // todo: cuz we're not tarfs we should be able to mount multiple volumes and this kinda makes it hard to do

KSTATUS Ext2AhciRead(KeExt2Volume* Volume, uint64_t RelLba, uint32_t Bytes, void** vBufOut, void** pBufOut, uint32_t* PagesOut);
KSTATUS Ext2ReadInode(KeExt2Volume* Vol, uint32_t ino, Ext2InoData* Out);
uint32_t Ext2AllocBlock(KeExt2Volume* Vol);
KSTATUS Ext2AhciWrite(KeExt2Volume* Volume, uint64_t RelLba, uint32_t Bytes, void* vBufIn, void** pBufOut, uint32_t* PagesOut);

// helper functions cuz like no point copy and pasting the same shit 20 times
static uint32_t Ext2ReadIndirect(KeExt2Volume* Vol, uint32_t BlockNum, uint32_t Idx) {
    if (BlockNum == 0) return 0;
    uint32_t Pages;
    uint64_t Lba = (uint64_t)BlockNum * Vol->BlockSize / 512;
    void* pBuf;
    void* vBuf;
    KSTATUS r = Ext2AhciRead(Vol, Lba, Vol->BlockSize, &vBuf, &pBuf, &Pages);
    if (r != KSUCCESS) return 0;
    uint32_t* ptr = (uint32_t*)vBuf;
    uint32_t Ret = ptr[Idx];
    PmmFreePages(pBuf, Pages);
    return Ret;
}

static uint32_t Ext2ResolveBlockIdx(KeExt2Volume* Vol, Ext2InoData* Inode, uint32_t LogicalBlks) {
    uint32_t PtrsPerBlk = Vol->BlockSize / 4;
    if (LogicalBlks < 12) {
        return (&Inode->Dbp[0])[LogicalBlks];
    }
    LogicalBlks -= 12;
    if (LogicalBlks < PtrsPerBlk) {
        return Ext2ReadIndirect(Vol, Inode->Sibp, LogicalBlks);
    }
    if (LogicalBlks < PtrsPerBlk * PtrsPerBlk) {
        uint32_t L1 = LogicalBlks / PtrsPerBlk;
        uint32_t L2 = LogicalBlks % PtrsPerBlk;
        uint32_t Mid = Ext2ReadIndirect(Vol, Inode->Dibp, L1);
        return Ext2ReadIndirect(Vol, Mid, L2);
    }
    LogicalBlks -= PtrsPerBlk * PtrsPerBlk;

    uint32_t L1 = LogicalBlks / (PtrsPerBlk * PtrsPerBlk);
    uint32_t Rem = LogicalBlks % (PtrsPerBlk * PtrsPerBlk);
    uint32_t L2 = Rem / PtrsPerBlk;
    uint32_t L3 = Rem % PtrsPerBlk;
    uint32_t Mid1 = Ext2ReadIndirect(Vol, Inode->Tibp, L1);
    uint32_t Mid2 = Ext2ReadIndirect(Vol, Mid1, L2);
    return Ext2ReadIndirect(Vol, Mid2, L3);
}

// zeroes a block and im too lazy to be actually doing this in ext2rslvblkidxalloc
static void Ext2ZeroBlock(KeExt2Volume* Vol, uint32_t BlockNum) {
    void* Zeroed = MmAllocate(Vol->BlockSize);
    memset(Zeroed, 0, Vol->BlockSize);
    void *pBuf; uint32_t Pages;
    Ext2AhciWrite(Vol, (uint64_t)BlockNum * Vol->BlockSize / 512, Vol->BlockSize, Zeroed, &pBuf, &Pages);
    PmmFreePages(pBuf, Pages);
    MmFree(Zeroed);
}

// wallahi the same thing as ext2readindirect but allocs a new block
static uint32_t Ext2RoAIndirectSl(KeExt2Volume* Vol, uint32_t IndirectBlockNum, uint32_t Idx) {
    uint64_t Lba = (uint64_t)IndirectBlockNum * Vol->BlockSize / 512;
    void *pBuf; 
    void *vBuf;
    uint32_t Pages;
    KSTATUS r;
    r = Ext2AhciRead(Vol, Lba, Vol->BlockSize, &vBuf, &pBuf, &Pages);
    KATTEMPT(r == KSUCCESS);
    uint32_t* Ptrs = (uint32_t*)vBuf;
    uint32_t Result = Ptrs[Idx];
    if (Result == 0) {
        Result = Ext2AllocBlock(Vol);
        Ptrs[Idx] = Result;

        void *pBuf2; 
        uint32_t Pages2;
        r = Ext2AhciWrite(Vol, Lba, Vol->BlockSize, vBuf, &pBuf2, &Pages2);
        KATTEMPT(r == KSUCCESS);
        PmmFreePages(pBuf2, Pages2);
    }
    PmmFreePages(pBuf, Pages);
    return Result;
}

static uint32_t Ext2RslvBlkIdxAlloc(KeExt2Volume* Vol, Ext2InoData* Inode, uint32_t LogicalBlks, int* InodeDirty) {
    uint32_t PtrsPerBlk = Vol->BlockSize / 4;

    if (LogicalBlks < 12) {
        if (Inode->Dbp[LogicalBlks] == 0) {
            Inode->Dbp[LogicalBlks] = Ext2AllocBlock(Vol);
            *InodeDirty = 1;
        }
        return Inode->Dbp[LogicalBlks];
    }
    LogicalBlks -= 12;

    if (LogicalBlks < PtrsPerBlk) {
        if (Inode->Sibp == 0) {
            Inode->Sibp = Ext2AllocBlock(Vol);
            Ext2ZeroBlock(Vol, Inode->Sibp);
            *InodeDirty = 1;
        }
        return Ext2RoAIndirectSl(Vol, Inode->Sibp, LogicalBlks);
    }
    if (LogicalBlks < PtrsPerBlk * PtrsPerBlk) {
        uint32_t L1 = LogicalBlks / PtrsPerBlk;
        uint32_t L2 = LogicalBlks % PtrsPerBlk;
        uint32_t Mid = Ext2RoAIndirectSl(Vol, Inode->Dibp, L1);
        return Ext2RoAIndirectSl(Vol, Mid, L2);
    }
    LogicalBlks -= PtrsPerBlk * PtrsPerBlk;

    uint32_t L1 = LogicalBlks / (PtrsPerBlk * PtrsPerBlk);
    uint32_t Rem = LogicalBlks % (PtrsPerBlk * PtrsPerBlk);
    uint32_t L2 = Rem / PtrsPerBlk;
    uint32_t L3 = Rem % PtrsPerBlk;
    uint32_t Mid1 = Ext2RoAIndirectSl(Vol, Inode->Tibp, L1);
    uint32_t Mid2 = Ext2RoAIndirectSl(Vol, Mid1, L2);
    return Ext2RoAIndirectSl(Vol, Mid2, L3);
}

// vfs-specific to make life easier :)
static uint32_t Ext2CountEntries(KeExt2Volume* Vol, uint32_t DirInode, uint32_t Depth) {
    if (Depth > 32) return 0;
    uint32_t Count = 0;
    Ext2InoData Inode;
    if (Ext2ReadInode(Vol, DirInode, &Inode) != KSUCCESS) return 0;
    uint32_t BlockCount = (Inode.SizeLow + Vol->BlockSize - 1) / Vol->BlockSize;
    for (uint32_t Block = 0; Block < BlockCount; Block++) { // im gonna cry bro i was so confused on why ts looped and it was bcz i never incremented
        uint32_t BlockNum = Ext2ResolveBlockIdx(Vol, &Inode, Block);
        if (BlockNum == 0) continue;
        uint64_t Lba = (uint64_t)BlockNum * Vol->BlockSize / 512;
        void* pBuf;
        void* vBuf;
        uint32_t Pages;
        KSTATUS r = Ext2AhciRead(Vol, Lba, Vol->BlockSize, &vBuf, &pBuf, &Pages);
        if (r != KSUCCESS) return r;
        uint8_t* Ptr = (uint8_t*)vBuf;
        uint8_t* End = Ptr + Vol->BlockSize;
        while (Ptr < End) {
            Ext2Dirent* Ent = (Ext2Dirent*)Ptr;
            if (Ent->Reclen == 0) break;
            if (Ent->Inode != 0 && !((strcmpl(Ent->Name, ".", 2) == 0) || (strcmpl(Ent->Name, "..", 3) == 0))) {
                Count++;
                if (Ent->FileType == 2) {
                    Count += Ext2CountEntries(Vol, Ent->Inode, Depth+1); // holy recursion
                }
            }

            Ptr += Ent->Reclen;
        }
        PmmFreePages(pBuf, Pages);
    }
    return Count;
}

// js go through the whole fs and create entries
static KSTATUS Ext2CreateVfsTable(KeExt2Volume* Vol, uint32_t DirInode, char* Path, uint32_t PathLen, uint32_t Depth) {
    // holy copy paste
    if (Depth > 32) return 0;
    Ext2InoData Inode;
    KSTATUS r;
    if (Ext2ReadInode(Vol, DirInode, &Inode) != KSUCCESS) return 0;
    uint32_t BlockCount = (Inode.SizeLow + Vol->BlockSize - 1) / Vol->BlockSize;
    for (uint32_t Block = 0; Block < BlockCount; Block++) {
        uint32_t BlockNum = Ext2ResolveBlockIdx(Vol, &Inode, Block);
        if (BlockNum == 0) continue;
        uint64_t Lba = (uint64_t)BlockNum * Vol->BlockSize / 512;
        void* pBuf;
        void* vBuf;
        uint32_t Pages;
        r = Ext2AhciRead(Vol, Lba, Vol->BlockSize, &vBuf, &pBuf, &Pages);
        if (r != KSUCCESS) return r;
        uint8_t* Ptr = (uint8_t*)vBuf;
        uint8_t* End = Ptr + Vol->BlockSize;
        while (Ptr < End) {
            Ext2Dirent* Ent = (Ext2Dirent*)Ptr;
            if (Ent->Reclen == 0) break;
            if (Ent->Inode != 0 && !((strcmpl(Ent->Name, ".", 2) == 0) || (strcmpl(Ent->Name, "..", 3) == 0))) {
                char PathBf[256];
                uint32_t Number = 0;
                memcpy(PathBf, Path, PathLen);
                Number = PathLen;
                if (Number == 0) PathBf[Number++] = '/';
                memcpy(PathBf + Number, Ent->Name, Ent->NameLen);
                Number += Ent->NameLen;
                PathBf[Number] = '\0';

                Ext2InoData Inode1;
                if (Ext2ReadInode(Vol, Ent->Inode, &Inode1) != KSUCCESS) KATTEMPT(0); // well shit
                VfsFile* Current = MmAllocate(sizeof(VfsFile));
                snprintf(Current->Path, VFS_MAX_ALLOWED_PATH, "%s:%s", Vol->Ext2Drive->Name, PathBf);
                if ((Inode1.TypePerm & 0xF000) == 0x8000) Current->Type = VFS_TYPE_FILE;
                else if ((Inode1.TypePerm & 0xF000) == 0x4000) Current->Type = VFS_TYPE_DIRECTORY;
                else Current->Type = 0x0;
                Current->Size = Inode1.SizeLow;
                Current->DrivePtr = Vol->Ext2Drive;
                Current->DriverRsv = Ent->Inode;
                if (Ent->FileType == 2) {
                    r = Ext2CreateVfsTable(Vol, Ent->Inode, PathBf, Number, Depth+1);
                }
                gCurrFileIdx++;
                Current->Next = Vol->Ext2Files;
                Vol->Ext2Files = Current;
            }

            Ptr += Ent->Reclen;
        }
        PmmFreePages(pBuf, Pages);
    }
    return r;
}
// wrapper
KSTATUS Ext2AhciRead(KeExt2Volume* Volume, uint64_t RelLba, uint32_t Bytes, void** vBufOut, void** pBufOut, uint32_t* PagesOut) {
    uint32_t Sectors = (Bytes + 511) / 512;
    uint32_t Pages = (Sectors * 512 + MMU_PAGE_SIZE - 1) / MMU_PAGE_SIZE;
    void* pBuf = PmmAllocatePages(Pages);
    if (!pBuf) return KOOMERR;
    void* vBuf = (void*)P2V(pBuf);
    memset(vBuf, 0, Pages * MMU_PAGE_SIZE);

    uint64_t AbsoluteLba = Volume->PartitionStartLba + RelLba;
    KSTATUS r = AhciPortRead(Volume->Port, AbsoluteLba, Sectors, (void*)pBuf);
    if (r != KSUCCESS) {
        PmmFreePages(pBuf, Pages);
        return r;
    }
    *vBufOut = vBuf;
    *pBufOut = pBuf;
    *PagesOut = Pages;
    return KSUCCESS;
}

KSTATUS Ext2AhciWrite(KeExt2Volume* Volume, uint64_t RelLba, uint32_t Bytes, void* vBufIn, void** pBufOut, uint32_t* PagesOut) {
    uint32_t Sectors = (Bytes + 511) / 512;
    uint32_t Pages = (Sectors * 512 + MMU_PAGE_SIZE - 1) / MMU_PAGE_SIZE;
    void* pBuf = PmmAllocatePages(Pages);
    if (!pBuf) return KOOMERR;
    void* vBuf = (void*)P2V(pBuf);
    memcpy((void*)vBuf, vBufIn, Bytes);
    uint64_t AbsoluteLba = Volume->PartitionStartLba + RelLba;
    KSTATUS r = AhciPortWrite(Volume->Port, AbsoluteLba, Sectors, (void*)pBuf);
    if (r != KSUCCESS) {
        PmmFreePages(pBuf, Pages);
        return r;
    }
    *pBufOut = pBuf;
    *PagesOut = Pages;
    return KSUCCESS;
}

// todo: vfs has a vfsdrive type so uh try integrate mounting like that.
KSTATUS Ext2Mount(int AhciPortNum, uint64_t PartitionStartLba, KeExt2Volume* VolOut) {
    VolOut->Port = AhciGetPort(AhciPortNum);
    if (!VolOut->Port) return KINVALID;
    VolOut->PartitionStartLba = PartitionStartLba;
    void* sbV;
    void* sbP;
    uint32_t Pages;
    KSTATUS r = Ext2AhciRead(VolOut, 2, 1024, &sbV, &sbP, &Pages);
    if (r != KSUCCESS) {
        return r;
    }
    Ext2Superblock* Sb = (Ext2Superblock*)sbV;
    Ext2SbDynRev* SbExt = (Ext2SbDynRev*)((uint64_t)Sb + sizeof(Ext2Superblock));
    if (Sb->Magic != 0xEF53) return KINVALID;
    VolOut->BlockSize = 1024 << Sb->LogBlkSz;
    VolOut->InodesPerGroup = Sb->InodesPerGroup;
    VolOut->BlocksPerGroup = Sb->BlksPerGroup;
    VolOut->InodeCount = Sb->InodeCount;
    VolOut->BlockCount = Sb->BlockCount;
    VolOut->FirstDataBlk = Sb->FirstDataBlk;
    VolOut->InodeSz = (Sb->Revision == EXT2_GOOD_OLD_REV) ? 128 : SbExt->InodeSz;
    VolOut->GroupsCount = (Sb->BlockCount + Sb->BlksPerGroup - 1) / Sb->BlksPerGroup;
    VolOut->SbPtr = Sb;
    VolOut->SbExtPtr = SbExt;
    uint64_t BgdtLba = ((uint64_t)(VolOut->FirstDataBlk + 1) * VolOut->BlockSize) / 512;
    uint32_t BgdtBytes = VolOut->GroupsCount * sizeof(Ext2BlkGroupDesc);
    uint32_t Pages2;
    r = Ext2AhciRead(VolOut, BgdtLba, BgdtBytes, &VolOut->Bgdtvbuf, &VolOut->Bgdtpbuf, &Pages2);
    if (r != KSUCCESS) {
        return r;
    }
    return KSUCCESS;
}

// pretty self explanatory
KSTATUS Ext2ReadInode(KeExt2Volume* Vol, uint32_t ino, Ext2InoData* Out) {
    if (ino == 0 || ino > Vol->InodeCount) return KINVALID;
    uint32_t Group = (ino - 1) / Vol->InodesPerGroup;
    uint32_t IndexInGroup = (ino - 1) % Vol->InodesPerGroup;
    //KATTEMPT(Group > Vol->GroupsCount);
    Ext2BlkGroupDesc* Bgdt = (Ext2BlkGroupDesc*)Vol->Bgdtvbuf;
    uint32_t InoTableBlk = Bgdt[Group].InodeTable;
    uint64_t Offset = (uint64_t)InoTableBlk * Vol->BlockSize + (uint64_t)IndexInGroup * Vol->InodeSz;
    uint64_t Lba = Offset / 512;
    uint32_t SectorOff = Offset % 512;
    void* pBuf;
    void* vBuf;
    uint32_t Pages;
    KSTATUS r= Ext2AhciRead(Vol, Lba, SectorOff + Vol->InodeSz, &vBuf, &pBuf, &Pages);
    if (r != KSUCCESS) return r;
    memcpy((void*)Out, (uint8_t*)vBuf + SectorOff, sizeof(Ext2InoData));
    PmmFreePages(pBuf, Pages);
    return KSUCCESS;
}

// this too
KSTATUS Ext2ReadRaw(KeExt2Volume* Vol, uint32_t ino, void* Out) {
    Ext2InoData* Inode = MmAllocate(sizeof(Ext2InoData));
    if (!Inode) return KOOMERR;
    KSTATUS r = Ext2ReadInode(Vol, ino, Inode);
    if (r != KSUCCESS) {
        MmFree(Inode);
        return r;
    }
    uint32_t FileSz = Inode->SizeLow;
    uint32_t BlockCount = (FileSz + Vol->BlockSize - 1) / Vol->BlockSize;
    // uint32_t Dbp[12] = {Inode->Dbp0, Inode->Dbp1, Inode->Dbp2, Inode->Dbp3, Inode->Dbp4, Inode->Dbp5, Inode->Dbp6, Inode->Dbp7, Inode->Dbp8, Inode->Dbp9, Inode->Dbp10, Inode->Dbp11};
    uint32_t RemainByte = FileSz;
    uint8_t* OutBuf = Out;
    for (uint32_t i = 0; i < BlockCount; i++) {
        uint32_t BlockNum = Ext2ResolveBlockIdx(Vol, Inode, i);
        if (BlockNum == 0) break;
        uint64_t Offset = (uint64_t)BlockNum * Vol->BlockSize;
        uint64_t Lba = Offset / 512;
        void* pBuf;
        void* vBuf;
        uint32_t Pages;
        KSTATUS r2 = Ext2AhciRead(Vol, Lba, Vol->BlockSize, &vBuf, &pBuf, &Pages);
        if (r2 != KSUCCESS) {
            printf("ext2: r2=%d\r\n", r2);
            KATTEMPT(0); // todo: handle correctly
        }
        uint32_t CpyLength = (RemainByte < Vol->BlockSize) ? RemainByte : Vol->BlockSize;
        memcpy(OutBuf, vBuf, CpyLength);
        OutBuf += CpyLength;
        RemainByte -= CpyLength;
        PmmFreePages(pBuf, Pages);
    }
    return KSUCCESS;
}
// this also
KSTATUS Ext2EnumDirent(KeExt2Volume* Vol, uint64_t Block) {
    uint64_t Offset = (uint64_t)Block * Vol->BlockSize;
    uint64_t Lba = Offset / 512;
    void* pBuf;
    void* vBuf;
    uint32_t Pages; 
    KSTATUS r = Ext2AhciRead(Vol, Lba, Vol->BlockSize, &vBuf, &pBuf, &Pages);
    if (r != KSUCCESS) return r;
    uint8_t* Ptr = (uint8_t*)vBuf;
    uint8_t* End = Ptr + Vol->BlockSize;
    while (Ptr < End) {
        Ext2Dirent* Ent = (Ext2Dirent*)Ptr;
        if (Ent->Inode != 0) {
            printf("ext2: inode %d: name=", Ent->Inode);
            for (int i = 0; i < Ent->NameLen; i++) _putchar(Ent->Name[i]);
            printf(" type=%d\r\n", Ent->FileType);
            if (Ent->FileType == 1) {
                Ext2InoData* Inode = MmAllocate(sizeof(Ext2InoData));
                if (!Inode) KATTEMPT(0);
                KSTATUS rX = Ext2ReadInode(Vol, Ent->Inode, Inode);
                if (rX != KSUCCESS) {
                    MmFree(Inode);
                    KATTEMPT(0);
                }
                uint8_t* Buf = MmAllocate(Inode->SizeLow);
                KSTATUS r2 = Ext2ReadRaw(Vol, Ent->Inode, (void*)Buf);
                if (r2 != KSUCCESS) {
                    MmFree(Buf);
                    continue;
                }
                printf("ext2: dump file contents: \r\n");
                for (int i = 0; i < Inode->SizeLow; i++) _putchar(Buf[i]);
                printf("\r\next2: end dump.\r\n");
                MmFree(Buf);
                MmFree(Inode);
            }
        }
        if (Ent->Reclen == 0) break;
        Ptr += Ent->Reclen;
    }
    PmmFreePages(pBuf, Pages);
    return KSUCCESS;
}

// write specific
uint32_t Ext2AllocBlock(KeExt2Volume* Vol) {
    Ext2BlkGroupDesc* Bgdt = (Ext2BlkGroupDesc*)Vol->Bgdtvbuf;
    for (uint32_t Group = 0; Group < Vol->GroupsCount; Group++) {
        if (Bgdt[Group].FreeBlocksCount > 0) {
            uint64_t Lba = (uint64_t)Bgdt[Group].BlockBitmap * Vol->BlockSize / 512;
            void* pBuf;
            void* vBuf;
            uint32_t Pages;
            KSTATUS r = Ext2AhciRead(Vol, Lba, Vol->BlockSize, &vBuf, &pBuf, &Pages);
            uint8_t* Bitmap = (uint8_t*)vBuf;
            // check for clear
            for (uint32_t i = 0; i < Vol->BlocksPerGroup; i++) {
                uint32_t ByteIdx = i / 8;
                uint32_t BitIdx = i % 8;
                if (!(Bitmap[ByteIdx] & (1<<BitIdx))) {
                    Bitmap[ByteIdx] |= (1<<BitIdx);
                    void* pBuf2;
                    uint32_t Pages;
                    r = Ext2AhciWrite(Vol, Lba, Vol->BlockSize, (void*)Bitmap, &pBuf2, &Pages);
                    if (r != KSUCCESS) KATTEMPT(0); // panic
                    PmmFreePages(pBuf2, Pages);
                    Bgdt[Group].FreeBlocksCount--;
                    Vol->SbPtr->FreeBlocksCount--;
                    // printf("ext2: alloc: Bgdt.FreeBlk=%d Sb.FreeBlk=%d\r\n", Bgdt[Group].FreeBlocksCount, Vol->SbPtr->FreeBlocksCount);
                    uint64_t BgdtLba = ((uint64_t)(Vol->FirstDataBlk + 1) * Vol->BlockSize) / 512;
                    uint32_t BgdtBytes = Vol->GroupsCount * sizeof(Ext2BlkGroupDesc);
                    r = Ext2AhciWrite(Vol, BgdtLba, BgdtBytes, (void*)Bgdt, &pBuf2, &Pages); // if bgdt spans multiple sectors ts will cause a problem
                    if (r != KSUCCESS) KATTEMPT(0); // handle
                    PmmFreePages(pBuf2, Pages);
                    r = Ext2AhciWrite(Vol, 2, 1024, (void*)Vol->SbPtr, &pBuf2, &Pages);
                    if (r != KSUCCESS) KATTEMPT(0); // also todo
                    uint32_t GlobalBlockNum = Group * Vol->BlocksPerGroup + i + Vol->FirstDataBlk;
                    return GlobalBlockNum;
                }
            }
            PmmFreePages(pBuf, Pages);
        }
    }
    return 0;
    
}
// same as allocblock
uint32_t Ext2AllocInode(KeExt2Volume* Vol, int IsDir) {
    Ext2BlkGroupDesc* Bgdt = (Ext2BlkGroupDesc*)Vol->Bgdtvbuf;
    for (uint32_t Group = 0; Group < Vol->InodesPerGroup; Group++) {
        if (Bgdt[Group].FreeInodesCount > 0) {
            uint64_t Lba = (uint64_t)Bgdt[Group].InodeBitmap * Vol->BlockSize / 512;
            void* pBuf;
            void* vBuf;
            uint32_t Pages;
            KSTATUS r = Ext2AhciRead(Vol, Lba, Vol->BlockSize, &vBuf, &pBuf, &Pages);
            uint8_t* Bitmap = (uint8_t*)vBuf;
            // check for clear
            for (uint32_t i = 0; i < Vol->InodesPerGroup; i++) {
                uint32_t ByteIdx = i / 8;
                uint32_t BitIdx = i % 8;
                uint32_t GlobalInodeNum = Group * Vol->InodesPerGroup + i + 1;
                uint32_t FirstIno;
                if (Vol->SbPtr->Revision == EXT2_DYNAMIC_REV) {
                    FirstIno = Vol->SbExtPtr->FirstIno;
                } else FirstIno = 11;
                if (GlobalInodeNum < FirstIno) continue; // dont hand out a reserved inode
                if (!(Bitmap[ByteIdx] & (1<<BitIdx))) {
                    Bitmap[ByteIdx] |= (1<<BitIdx);
                    void* pBuf2;
                    uint32_t Pages;
                    r = Ext2AhciWrite(Vol, Lba, Vol->BlockSize, (void*)Bitmap, &pBuf2, &Pages);
                    if (r != KSUCCESS) KATTEMPT(0); // panic
                    PmmFreePages(pBuf2, Pages);
                    Bgdt[Group].FreeInodesCount--;
                    Vol->SbPtr->FreeInodeCount--;
                    if (IsDir) Bgdt[Group].UsedDirsCount++;
                    // printf("ext2: alloc: Bgdt.FreeIno=%d Sb.FreeIno=%d\r\n", Bgdt[Group].FreeInodesCount, Vol->SbPtr->FreeInodeCount);
                    uint64_t BgdtLba = ((uint64_t)(Vol->FirstDataBlk + 1) * Vol->BlockSize) / 512;
                    uint32_t BgdtBytes = Vol->GroupsCount * sizeof(Ext2BlkGroupDesc);
                    r = Ext2AhciWrite(Vol, BgdtLba, BgdtBytes, (void*)Bgdt, &pBuf2, &Pages); // if bgdt spans multiple sectors ts will cause a problem
                    if (r != KSUCCESS) KATTEMPT(0); // handle
                    PmmFreePages(pBuf2, Pages);
                    r = Ext2AhciWrite(Vol, 2, 1024, (void*)Vol->SbPtr, &pBuf2, &Pages);
                    if (r != KSUCCESS) KATTEMPT(0); // also todo
                    return GlobalInodeNum;
                }
            }
            PmmFreePages(pBuf, Pages);
        }
    }
    return 0;
    
}
KSTATUS Ext2FreeInode(KeExt2Volume* Vol, uint32_t GlobalInodeNum, int IsDir) {
    Ext2BlkGroupDesc* Bgdt = (Ext2BlkGroupDesc*)Vol->Bgdtvbuf;
    uint32_t RelNode = GlobalInodeNum - 1;
    uint32_t Group = RelNode / Vol->InodesPerGroup;
    uint32_t LocalIdx = RelNode % Vol->InodesPerGroup;
    uint64_t BitmapLba = (uint64_t)Bgdt[Group].InodeBitmap * Vol->InodeSz / 512;
    void* pBuf;
    void* vBuf;
    uint32_t Pages;
    KSTATUS r = Ext2AhciRead(Vol, BitmapLba, Vol->BlockSize, &vBuf, &pBuf, &Pages);
    if (r != KSUCCESS) return r;
    uint8_t* Bitmap = (uint8_t*)vBuf;
    uint32_t ByteIdx = LocalIdx / 8;
    uint32_t BitIdx = LocalIdx % 8;
    if (!(Bitmap[ByteIdx] & (1 << BitIdx))) {
        PmmFreePages(pBuf, Pages);
        return KDOUBLEFREE;
    }
    Bitmap[ByteIdx] &= ~(1 << BitIdx);
    void* pBuf2;
    uint32_t Pages2;
    r = Ext2AhciWrite(Vol, BitmapLba, Vol->BlockSize, (void*)Bitmap, &pBuf2, &Pages2);
    if (r != KSUCCESS) {
        PmmFreePages(pBuf, Pages);
        return r;
    }
    Bgdt[Group].FreeInodesCount++;
    Vol->SbPtr->FreeInodeCount++;
    if (IsDir) Bgdt[Group].UsedDirsCount--;
    // printf("ext2: free: Bgdt.FreeIno=%d Sb.FreeIno=%d\r\n", Bgdt[Group].FreeInodesCount, Vol->SbPtr->FreeInodeCount);
    uint64_t BgdtLba = ((uint64_t)(Vol->FirstDataBlk + 1) * Vol->BlockSize) / 512;
    uint32_t BgdtBytes = Vol->GroupsCount * sizeof(Ext2BlkGroupDesc);
    r = Ext2AhciWrite(Vol, BgdtLba, BgdtBytes, (void*)Bgdt, &pBuf2, &Pages); // if bgdt spans multiple sectors ts will cause a problem
    if (r != KSUCCESS) KATTEMPT(0); // handle
    PmmFreePages(pBuf2, Pages);
    r = Ext2AhciWrite(Vol, 2, 1024, (void*)Vol->SbPtr, &pBuf2, &Pages);
    if (r != KSUCCESS) KATTEMPT(0); // also todo
    return KSUCCESS;
}

KSTATUS Ext2FreeBlock(KeExt2Volume* Vol, uint32_t GlobalBlockNum) {
    Ext2BlkGroupDesc* Bgdt = (Ext2BlkGroupDesc*)Vol->Bgdtvbuf;
    uint32_t RelBlock = GlobalBlockNum - Vol->FirstDataBlk;
    uint32_t Group = RelBlock / Vol->BlocksPerGroup;
    uint32_t LocalIdx = RelBlock % Vol->BlocksPerGroup;
    uint64_t BitmapLba = (uint64_t)Bgdt[Group].BlockBitmap * Vol->BlockSize / 512;
    void* pBuf;
    void* vBuf;
    uint32_t Pages;
    KSTATUS r = Ext2AhciRead(Vol, BitmapLba, Vol->BlockSize, &vBuf, &pBuf, &Pages);
    if (r != KSUCCESS) return r;
    uint8_t* Bitmap = (uint8_t*)vBuf;
    uint32_t ByteIdx = LocalIdx / 8;
    uint32_t BitIdx = LocalIdx % 8;
    if (!(Bitmap[ByteIdx] & (1 << BitIdx))) {
        PmmFreePages(pBuf, Pages);
        return KDOUBLEFREE;
    }
    Bitmap[ByteIdx] &= ~(1 << BitIdx);
    void* pBuf2;
    uint32_t Pages2;
    r = Ext2AhciWrite(Vol, BitmapLba, Vol->BlockSize, (void*)Bitmap, &pBuf2, &Pages2);
    if (r != KSUCCESS) {
        PmmFreePages(pBuf, Pages);
        return r;
    }
    Bgdt[Group].FreeBlocksCount++;
    Vol->SbPtr->FreeBlocksCount++;
    // printf("ext2: free: Bgdt.FreeBlk=%d Sb.FreeBlk=%d\r\n", Bgdt[Group].FreeBlocksCount, Vol->SbPtr->FreeBlocksCount);
    uint64_t BgdtLba = ((uint64_t)(Vol->FirstDataBlk + 1) * Vol->BlockSize) / 512;
    uint32_t BgdtBytes = Vol->GroupsCount * sizeof(Ext2BlkGroupDesc);
    r = Ext2AhciWrite(Vol, BgdtLba, BgdtBytes, (void*)Bgdt, &pBuf2, &Pages); // if bgdt spans multiple sectors ts will cause a problem
    if (r != KSUCCESS) KATTEMPT(0); // handle
    PmmFreePages(pBuf2, Pages);
    r = Ext2AhciWrite(Vol, 2, 1024, (void*)Vol->SbPtr, &pBuf2, &Pages);
    if (r != KSUCCESS) KATTEMPT(0); // also todo
    return KSUCCESS;
}
// a bug in this caused so much fucking issues and waste of my time
KSTATUS Ext2WriteInode(KeExt2Volume* Vol, uint32_t ino, Ext2InoData* In) {
    if (ino == 0 || ino > Vol->InodeCount) return KINVALID;
    uint32_t Group = (ino - 1) / Vol->InodesPerGroup;
    uint32_t IndexInGroup = (ino - 1) % Vol->InodesPerGroup;
    Ext2BlkGroupDesc* Bgdt = (Ext2BlkGroupDesc*)Vol->Bgdtvbuf;
    uint32_t InoTableBlk = Bgdt[Group].InodeTable;
    uint64_t Offset = (uint64_t)InoTableBlk * Vol->BlockSize + (uint64_t)IndexInGroup * Vol->InodeSz;
    uint64_t Lba = Offset / 512;
    uint32_t SectorOff = Offset % 512;
    void *vBuf, *pBuf;
    uint32_t Pages;
    KSTATUS r = Ext2AhciRead(Vol, Lba, SectorOff + Vol->InodeSz, &vBuf, &pBuf, &Pages);
    if (r != KSUCCESS) return r;
    memcpy((uint8_t*)vBuf + SectorOff, In, sizeof(Ext2InoData));
    void* pBuf2;
    uint32_t Pages2;
    r = Ext2AhciWrite(Vol, Lba, SectorOff + Vol->InodeSz, vBuf, &pBuf2, &Pages2);
    PmmFreePages(pBuf, Pages);
    if (r != KSUCCESS) { if(pBuf2) PmmFreePages(pBuf2, Pages2); return r; }
    PmmFreePages(pBuf2, Pages2);
    return KSUCCESS;
}

KSTATUS Ext2InsertDirent(KeExt2Volume* Vol, uint32_t ParentInode, uint32_t PInode, char* Name, uint32_t NameLen, uint8_t FileType) {
    uint32_t NeededLen = 8 + NameLen;
    NeededLen = (NeededLen + 3) & ~3;
    Ext2InoData Inode;
    KSTATUS r;
    if (Ext2ReadInode(Vol, ParentInode, &Inode) != KSUCCESS) return 0;
    uint32_t BlockCount = (Inode.SizeLow + Vol->BlockSize - 1) / Vol->BlockSize;
    for (uint32_t Block = 0; Block < BlockCount; Block++) {
        uint32_t BlockNum = Ext2ResolveBlockIdx(Vol, &Inode, Block);
        if (BlockNum == 0) continue;
        uint64_t Lba = (uint64_t)BlockNum * Vol->BlockSize / 512;
        void* pBuf;
        void* vBuf;
        uint32_t Pages;
        r = Ext2AhciRead(Vol, Lba, Vol->BlockSize, &vBuf, &pBuf, &Pages);
        if (r != KSUCCESS) return r;
        uint8_t* Ptr = (uint8_t*)vBuf;
        uint8_t* End = Ptr + Vol->BlockSize;
        Ext2Dirent* Ent = (Ext2Dirent*)Ptr;
        while (Ptr < End) {
            uint32_t MinSz = 8 + Ent->NameLen;
            MinSz = (MinSz + 3) & ~3;
            if (Ent->Reclen == 0) break;
            if (Ent->Reclen > MinSz) {
                if (Ent->Reclen - MinSz >= NeededLen) {
                    uint32_t OldReclen = Ent->Reclen;
                    Ent->Reclen = MinSz;
                    Ext2Dirent* NewEnt = (Ext2Dirent*)((uint8_t*)Ent + MinSz);
                    NewEnt->Inode = PInode;
                    NewEnt->Reclen = OldReclen - MinSz;
                    NewEnt->NameLen = NameLen;
                    NewEnt->FileType = FileType;
                    memcpy(NewEnt->Name, Name, NameLen);
                    uint32_t Pages2;
                    void* pBuf2;
                    r = Ext2AhciWrite(Vol, Lba, Vol->BlockSize, vBuf, &pBuf2, &Pages2);
                    // printf("ext2: Lba=%lu Vol->BlockSize=%d OldReclen=%d MinSz=%d NeededLen=%d\r\n", Lba, Vol->BlockSize, OldReclen, MinSz, NeededLen);
                    if (r != KSUCCESS) KATTEMPT(0);
                    PmmFreePages(pBuf, Pages);
                    return KSUCCESS;
                }
            }
            Ptr += Ent->Reclen;
        }
        PmmFreePages(pBuf, Pages);
        uint32_t NewBlockNum = Ext2AllocBlock(Vol);
        uint64_t Lba2 = (uint64_t)NewBlockNum * Vol->BlockSize / 512;
        Ext2Dirent* NewEnt = (Ext2Dirent*)MmAllocate(Vol->BlockSize);
        memset(NewEnt, 0, Vol->BlockSize);
        NewEnt->Inode = PInode;
        NewEnt->Reclen = Vol->BlockSize;
        NewEnt->NameLen = NameLen;
        NewEnt->FileType = FileType;
        memcpy(NewEnt->Name, Name, NameLen);
        uint32_t Pages2;
        void* pBuf2;
        r = Ext2AhciWrite(Vol, Lba2, Vol->BlockSize, NewEnt, &pBuf2, &Pages2);
        if (r != KSUCCESS) KATTEMPT(0);
        for (int i = 0; i < 12; i++) if (Inode.Dbp[i] == 0) Inode.Dbp[i] = NewBlockNum;
        Inode.SizeLow += Vol->BlockSize;
        r = Ext2WriteInode(Vol, ParentInode, &Inode);
        if (r != KSUCCESS) KATTEMPT(0);
        PmmFreePages(pBuf2, Pages2);
        return KSUCCESS;
    }
    return KFAIL;
}

KSTATUS Ext2CreateFile(KeExt2Volume* Vol, uint32_t ParentInode, char* Name, int IsDir, uint32_t* InodeOut) {
    uint32_t Inode = Ext2AllocInode(Vol, IsDir);
    if (Inode == 0) return KFAIL;
    Ext2InoData NewData;
    memset(&NewData, 0, sizeof(Ext2InoData));
    NewData.TypePerm = IsDir ? (0x4000 | 0755) : (0x8000 | 0644);
    NewData.SizeLow = 0;
    NewData.HardlinkCount = IsDir ? 2 : 1;
    NewData.CreationTime = 0; // dont have rtc so todo
    NewData.LastModification = NewData.CreationTime;
    if (IsDir) {
        uint32_t DirBlk = Ext2AllocBlock(Vol);
        uint8_t* BlkData = MmAllocate(Vol->BlockSize);
        Ext2Dirent* Dot =  (Ext2Dirent*)BlkData;
        Dot->Inode = Inode;
        Dot->Reclen = 12;
        Dot->NameLen = 1;
        Dot->FileType = 2;
        Dot->Name[0] = '.';
        Ext2Dirent* DotDot = (Ext2Dirent*)((uint64_t)BlkData + 12);
        DotDot->Inode = Inode;
        DotDot->Reclen = Vol->BlockSize - 12; // take up rest of block
        DotDot->NameLen = 2;
        DotDot->FileType = 2;
        Dot->Name[0] = '.';
        Dot->Name[1] = '.';
        void* pBuf;
        uint32_t Pages;
        KSTATUS r = Ext2AhciWrite(Vol, (DirBlk * Vol->BlockSize / 512), Vol->BlockSize, (void*)BlkData, &pBuf, &Pages);
        if (r != KSUCCESS) {
            MmFree(BlkData);
            Ext2FreeInode(Vol, Inode, IsDir);
            Ext2FreeBlock(Vol, DirBlk);
            return r;
        }
        NewData.Dbp[0] = DirBlk;
        NewData.SizeLow = Vol->BlockSize;
        r = Ext2WriteInode(Vol, Inode, &NewData);
        if (r != KSUCCESS) {
            MmFree(BlkData);
            Ext2FreeInode(Vol, Inode, IsDir);
            Ext2FreeBlock(Vol, DirBlk);
            return r;
        }
    } else {
        KSTATUS r = Ext2WriteInode(Vol, Inode, &NewData);
        if (r != KSUCCESS) {
            Ext2FreeInode(Vol, Inode, IsDir);
            return r;
        }
    } 
    // holy if statement js merge it in with the other its never this deep
    if (IsDir) {
        // increase link count
        Ext2InoData ParentData;
        KSTATUS r = Ext2ReadInode(Vol, ParentInode, &ParentData);
        if (r != KSUCCESS) {
            // ???
            return r;
        }
        ParentData.HardlinkCount++;
        r = Ext2WriteInode(Vol, ParentInode, &ParentData);
        if (r != KSUCCESS) {
            // not really sure how to handle
            return r;
        }

    }
    KSTATUS r = Ext2InsertDirent(Vol, ParentInode, Inode, Name, strlen(Name), IsDir ? 2 : 1);
    if (r != KSUCCESS) {
        // atleast we tried
        return r;
    }
    *InodeOut = Inode;
    return KSUCCESS;
}
// function js to make writing Ext2VfsWrite  a bit easier
KSTATUS Ext2WriteFile(KeExt2Volume* Vol, uint32_t InodeNum, uint8_t* Buffer, uint32_t Bytes, uint32_t Offset) {
    Ext2InoData Inode;
    KSTATUS r = Ext2ReadInode(Vol, InodeNum, &Inode);
    if (r != KSUCCESS) {
        return KFAIL;
    }
    uint32_t StartBlock = Offset / Vol->BlockSize;
    uint32_t EndBlock = (Offset + Bytes - 1) / Vol->BlockSize;
    int InodeDirty = 0;
    uint32_t Rem = Bytes;
    uint8_t* Src = Buffer;
    for (uint32_t Block = StartBlock; Block <= EndBlock; Block++) {
        uint32_t PhysBlk = Ext2RslvBlkIdxAlloc(Vol, &Inode, Block, &InodeDirty);
        printf("ext2: Block %d -> PhysBlk %d (InodeDirty=%d)\r\n", Block, PhysBlk, InodeDirty);
        uint64_t Lba = (uint64_t)PhysBlk * Vol->BlockSize / 512;
        void* pBuf;
        void* vBuf;
        uint32_t Pages;
        r = Ext2AhciRead(Vol, Lba, Vol->BlockSize, &vBuf, &pBuf, &Pages);
        KATTEMPT(r == KSUCCESS);
        uint32_t BlkStarto = (Block == StartBlock) ? (Offset % Vol->BlockSize) : 0;
        uint32_t ThisChunkSz = Vol->BlockSize - BlkStarto;
        if (ThisChunkSz > Rem) ThisChunkSz = Rem;
        memcpy((uint8_t*)vBuf + BlkStarto, Src, ThisChunkSz);
        void* pBuf2;
        uint32_t Pages2;
        r = Ext2AhciWrite(Vol, Lba, Vol->BlockSize, vBuf, &pBuf2, &Pages2);
        KATTEMPT(r == KSUCCESS);
        PmmFreePages(pBuf, Pages);
        PmmFreePages(pBuf2, Pages2);
        Src += ThisChunkSz;
        Rem -= ThisChunkSz;
    }
    if (Offset + Bytes > Inode.SizeLow) {
        // update size if we have more size now
        Inode.SizeLow = Offset + Bytes;
        InodeDirty = 1;
    }
    if (InodeDirty) {
        KATTEMPT(Ext2WriteInode(Vol, InodeNum, &Inode) == KSUCCESS); // update inode info if anything changed
    }
    return KSUCCESS;
}
// js goes through directories until matches path and returns inode
// still keeping the DriverRsv though cuz its faster to look that up instead of doing this
// every time we want to get inode for a path
static uint32_t Ext2ResolvePath(KeExt2Volume* Vol, const char* Path) {
    uint32_t CurrentInode = EXT2_ROOT_INODE;
    while (*Path == '/') Path++;
    if (*Path == '\0') return CurrentInode;
    char Component[256];
    while (*Path) {
        uint32_t Len = 0;
        while (Path[Len] && Path[Len] != '/' && Len < sizeof(Component) - 1) Len++;
        memcpy(Component, Path, Len);
        Component[Len] = '\0';
        Ext2InoData DirInode;
        if (Ext2ReadInode(Vol, CurrentInode, &DirInode) != KSUCCESS) return 0;
        if ((DirInode.TypePerm & 0xF000) != 0x4000) return 0;
        uint32_t NextInode = 0;
        uint32_t BlockCount = (DirInode.SizeLow + Vol->BlockSize - 1) / Vol->BlockSize;
        for (uint32_t b = 0; b < BlockCount && NextInode == 0; b++) {
            uint32_t BlockNum = Ext2ResolveBlockIdx(Vol, &DirInode, b);
            if (BlockNum == 0) continue;
            uint64_t Lba = (uint64_t)BlockNum * Vol->BlockSize / 512;
            void *vBuf, *pBuf; uint32_t Pages;
            if (Ext2AhciRead(Vol, Lba, Vol->BlockSize, &vBuf, &pBuf, &Pages) != KSUCCESS) continue;
            uint8_t* Ptr = (uint8_t*)vBuf;
            uint8_t* End = Ptr + Vol->BlockSize;
            while (Ptr < End) {
                Ext2Dirent* Ent = (Ext2Dirent*)Ptr;
                if (Ent->Reclen == 0) break;
                if (Ent->Inode != 0 && Ent->NameLen == Len && memcmp(Ent->Name, Component, Len) == 0) {
                    NextInode = Ent->Inode;
                    break;
                }
                Ptr += Ent->Reclen;
            }
            PmmFreePages(pBuf, Pages);
        }
        if (NextInode == 0) return 0;
        CurrentInode = NextInode;
        Path += Len;
        while (*Path == '/') Path++;
    }
    return CurrentInode;
}

VfsFile* Ext2VfsFindFile(const char* Path) {
    VfsFile* Current = gVolume->Ext2Files;
    for (int i = 0; i < gCurrFileIdx; i++) {
        if (strcmpl(Current->Path, Path, VFS_MAX_ALLOWED_PATH) == 0) return Current;
        Current = Current->Next;
    }
    return NULL;
}

int Ext2VfsRead(VfsFile* File, void* OutBuf, size_t Bytes, uint64_t Offset) {
    (void)Offset; // todo
    if (File->Type != VFS_TYPE_FILE) return -1;
    uint32_t Inode = File->DriverRsv;
    KSTATUS r = Ext2ReadRaw(gVolume, Inode, OutBuf);
    if (r != KSUCCESS) return -1;
    else return 0;
}

int Ext2VfsWrite(VfsFile* File, const void* InBuf, size_t Bytes, uint64_t Offset) {
    if (File->Type != VFS_TYPE_FILE) return -1;
    uint32_t Inode = File->DriverRsv;
    KSTATUS r = Ext2WriteFile(gVolume, Inode, InBuf, Bytes, (uint32_t)Offset);
    if (r != KSUCCESS) return -1;
    if (Bytes + Offset > File->Size) File->Size = (uint64_t)Bytes + Offset;
    return Bytes;
}

int Ext2VfsCreate(const char* Path, int Type) {
    char* DriverPath = VfsRemoveFormatPath(Path);
    if (DriverPath[0] == '\0') return -1;
    if (DriverPath[0] == '/' && DriverPath[1] == '\0') return -1;
    // split parent path and filename
    int LastSlashIdx = -1;
    for (int i = 0; DriverPath[i] != '\0'; i++) {
        if (DriverPath[i] == '/') LastSlashIdx = i;
    }
    char ParentPath[256];
    const char* Name;
    if (LastSlashIdx <= 0) {
        ParentPath[0] = '/';
        ParentPath[1] = '\0';
        Name = (LastSlashIdx == 0) ? (DriverPath + 1) : DriverPath;
    } else {
        memcpy(ParentPath, DriverPath, LastSlashIdx);
        ParentPath[LastSlashIdx] = '\0';
        Name = DriverPath + LastSlashIdx + 1;
    }
    uint32_t ParentInode = Ext2ResolvePath(gVolume, ParentPath);
    if (ParentInode == 0) return -1;
    uint32_t InodeOut;
    KSTATUS r = Ext2CreateFile(gVolume, ParentInode, Name, Type, &InodeOut);
    if (r != KSUCCESS) return -1;
    // also make vfs recognize it
    VfsFile* File = MmAllocate(sizeof(VfsFile));
    strlcpy(File->Path, Path, VFS_MAX_ALLOWED_PATH);
    File->Type = (Type == 1) ? VFS_TYPE_DIRECTORY : VFS_TYPE_FILE;
    File->DrivePtr = gVolume->Ext2Drive;
    File->DriverRsv = InodeOut;
    File->Size = 0;
    File->Perms = 0;
    File->Next = gVolume->Ext2Files;
    gVolume->Ext2Files = File;
    gCurrFileIdx++;
    return 0;
}

// js copy and pasted it from tarfs cuz it was similar enough
int Ext2VfsReadDir(VfsFile* File, VfsDirEntry* OutDirEnt, int Index) {
    uint64_t DirLen = strlen(File->Path);
    int matches = 0;
    VfsFile* Current = gVolume->Ext2Files;
    for (int i = 0; i < gCurrFileIdx; i++) {
        if (Current->Path[0] == '\0') {
            break; 
        }
        const char* EntryPath = Current->Path;
        if (strcmpl(EntryPath, File->Path, DirLen) != 0) continue;
        if (strcmpl(EntryPath, File->Path, DirLen) == 0 && DirLen == strlen(EntryPath)) continue;
        const char* RelativePart = EntryPath + DirLen;
        if (RelativePart[0] == '/') RelativePart++;
        int IsNest = 0;
        int j = 0;
        while (RelativePart[j] != '\0') {
            if (RelativePart[j] == '/') {
                if (RelativePart[j + 1] != '\0') {
                    IsNest = 1;
                    break;
                }
            }
            j++;
        }
        if (IsNest) continue;
        if (matches == Index) {
            OutDirEnt->Id = i;
            OutDirEnt->Type = Current->Type;
            memcpy(OutDirEnt->Name, RelativePart, strlen(RelativePart));
            memcpy(OutDirEnt->Path, EntryPath, strlen(EntryPath));
            OutDirEnt->Name[strlen(RelativePart)] = 0;
            OutDirEnt->Path[strlen(EntryPath)] = 0;
            return 1;
        }
        matches++;
        Current = Current->Next;
    }
    return 0;
}

int Ext2VfsGetFileSize(VfsFile* File) {
    uint32_t InodeNum = File->DriverRsv;
    Ext2InoData Inode;
    KSTATUS r = Ext2ReadInode(gVolume, InodeNum, &Inode);
    KATTEMPT(r == KSUCCESS);
    return Inode.SizeLow;
}

void Ext2SbInit(uint64_t lba, uint64_t partnum) {
    KeExt2Volume* Vol = MmAllocate(sizeof(KeExt2Volume));
    Vol->Ext2Drive = MmAllocate(sizeof(VfsDrive));
    Vol->Ext2Drive->DriverOps = MmAllocate(sizeof(VfsDriverOperation));
    memset(Vol->Ext2Drive->DriverOps, 0, sizeof(VfsDriverOperation));
    Vol->Ext2Drive->DriverOps->FindFile = Ext2VfsFindFile;
    Vol->Ext2Drive->DriverOps->Open = NULL; // dont really need these currently
    Vol->Ext2Drive->DriverOps->Close = NULL; // same goes
    Vol->Ext2Drive->DriverOps->Write = Ext2VfsWrite;
    Vol->Ext2Drive->DriverOps->Read = Ext2VfsRead;
    Vol->Ext2Drive->DriverOps->ReadDir = Ext2VfsReadDir;
    Vol->Ext2Drive->DriverOps->Create = Ext2VfsCreate;
    Vol->Ext2Drive->DriverOps->GetFileSize = Ext2VfsGetFileSize;
    snprintf(Vol->Ext2Drive->Name, 7, "ext2_%d", partnum);
    KSTATUS r = Ext2Mount(0, lba, Vol);
    if (r != KSUCCESS) {
        printf("ext2: failed to do lower level mount.\r\n");
        MmFree(Vol);
        return;
    }
    Ext2InoData* Root = MmAllocate(sizeof(Ext2InoData));
    r = Ext2ReadInode(Vol, EXT2_ROOT_INODE, Root);
    if (r != KSUCCESS) {
        printf("ext2: failed to read inode of root.\r\n");
        MmFree(Root);
        MmFree(Vol);
        return;
    }
    uint64_t Count = Ext2CountEntries(Vol, EXT2_ROOT_INODE, 0);
    Vol->Ext2Files = MmAllocate(sizeof(VfsFile));
    // manually make root node
    snprintf(Vol->Ext2Files->Path, VFS_MAX_ALLOWED_PATH, "%s:%s", Vol->Ext2Drive->Name, "/");
    Vol->Ext2Files->DrivePtr = Vol->Ext2Drive;
    Vol->Ext2Files->Size = Vol->BlockSize;
    Vol->Ext2Files->Type = VFS_TYPE_DIRECTORY;
    Vol->Ext2Files->Next = NULL;
    gCurrFileIdx++;
    r = Ext2CreateVfsTable(Vol, EXT2_ROOT_INODE, "/", 1, 0);
    VfsAddDriveToList(Vol->Ext2Drive);
    gVolume = Vol;
}