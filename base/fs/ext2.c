#include <fs/ext2.h>
#include <printfwrapper.h>
#include <disk/ahci.h>
#include <mm/pmm.h>
#include <mm/heap.h>
#include <memory.h>

// wrapper
KSTATUS Ext2AhciRead(KeExt2Volume* Volume, uint64_t RelLba, uint32_t Bytes, void** vBufOut, void** pBufOut) {
    uint32_t Sectors = (Bytes + 511) / 512;
    uint32_t Pages = (Sectors * 512 + MMU_PAGE_SIZE - 1) / MMU_PAGE_SIZE;
    void* pBuf = PmmAllocatePages(Pages);
    if (!pBuf) return KOOMERR;
    void* vBuf = (void*)P2V(pBuf);
    memset(vBuf, 0, Pages * MMU_PAGE_SIZE);

    uint64_t AbsoluteLba = Volume->PartitionStartLba + RelLba;
    KSTATUS r = AhciPortRead(Volume->Port, AbsoluteLba, Sectors, (void*)pBuf);
    if (r != KSUCCESS) {
        PmmFree(pBuf);
        return r;
    }
    *vBufOut = vBuf;
    *pBufOut = pBuf;
    return KSUCCESS;
}

// todo: vfs has a vfsdrive type so uh try integrate mounting like that.
KSTATUS Ext2Mount(int AhciPortNum, uint64_t PartitionStartLba, KeExt2Volume* VolOut) {
    VolOut->Port = AhciGetPort(AhciPortNum);
    if (!VolOut->Port) return KINVALID;
    VolOut->PartitionStartLba = PartitionStartLba;
    void* sbV;
    void* sbP;
    KSTATUS r = Ext2AhciRead(VolOut, 2, 1024, &sbV, &sbP);
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
    PmmFree(sbP);
    uint64_t BgdtLba = ((uint64_t)(VolOut->FirstDataBlk + 1) * VolOut->BlockSize) / 512;
    uint32_t BgdtBytes = VolOut->GroupsCount * sizeof(Ext2BlkGroupDesc);
    r = Ext2AhciRead(VolOut, BgdtLba, BgdtBytes, &VolOut->Bgdtvbuf, &VolOut->Bgdtpbuf);
    if (r != KSUCCESS) {
        return r;
    }
    return KSUCCESS;
}

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
    KSTATUS r= Ext2AhciRead(Vol, Lba, SectorOff + Vol->InodeSz, &vBuf, &pBuf);
    if (r != KSUCCESS) return r;
    memcpy((void*)Out, (uint8_t*)vBuf + SectorOff, sizeof(Ext2InoData));
    PmmFree(pBuf);
    return KSUCCESS;
}
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
    uint32_t Dbp[12] = {Inode->Dbp0, Inode->Dbp1, Inode->Dbp2, Inode->Dbp3, Inode->Dbp4, Inode->Dbp5, Inode->Dbp6, Inode->Dbp7, Inode->Dbp8, Inode->Dbp9, Inode->Dbp10, Inode->Dbp11};
    uint32_t RemainByte = FileSz;
    uint8_t* OutBuf = Out;
    for (uint32_t i = 0; i < BlockCount; i++) {
        uint32_t BlockNum = Dbp[i];
        if (BlockNum == 0) break;
        uint64_t Offset = (uint64_t)BlockNum * Vol->BlockSize;
        uint64_t Lba = Offset / 512;
        void* pBuf;
        void* vBuf;
        KSTATUS r2 = Ext2AhciRead(Vol, Lba, Vol->BlockSize, &vBuf, &pBuf);
        if (r2 != KSUCCESS) {
            KATTEMPT(0); // todo: handle correctly
        }
        uint32_t CpyLength = (RemainByte < Vol->BlockSize) ? RemainByte : Vol->BlockSize;
        memcpy(OutBuf, vBuf, CpyLength);
        OutBuf += CpyLength;
        RemainByte -= CpyLength;
        PmmFree(pBuf);
    }
    return KSUCCESS;
}
KSTATUS Ext2EnumDirent(KeExt2Volume* Vol, uint64_t Block) {
    uint64_t Offset = (uint64_t)Block * Vol->BlockSize;
    uint64_t Lba = Offset / 512;
    void* pBuf;
    void* vBuf;
    KSTATUS r = Ext2AhciRead(Vol, Lba, Vol->BlockSize, &vBuf, &pBuf);
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
                uint8_t* Buf = MmAllocate(128);
                KSTATUS r2 = Ext2ReadRaw(Vol, Ent->Inode, (void*)Buf);
                if (r2 != KSUCCESS) {
                    MmFree(Buf);
                    continue;
                }
                printf("ext2: dump file contents: \r\n");
                for (int i = 0; i < 128; i++) _putchar(Buf[i]);
                printf("\r\next2: end dump.\r\n");
                MmFree(Buf);
            }
        }
        if (Ent->Reclen == 0) break;
        Ptr += Ent->Reclen;
    }
    PmmFree(pBuf);
    return KSUCCESS;
}

void Ext2SbDump(uint64_t lba) {
    KeExt2Volume* Vol = MmAllocate(sizeof(KeExt2Volume));
    KSTATUS r = Ext2Mount(0, lba, Vol);
    if (r != KSUCCESS) {
        printf("ext2: failed to do lower level mount.\r\n");
    }
    Ext2InoData* Root = MmAllocate(sizeof(Ext2InoData));
    r = Ext2ReadInode(Vol, EXT2_ROOT_INODE, Root);
    if (r != KSUCCESS) {
        printf("ext2: failed to read inode of root.\r\n");
        MmFree(Root);
        return;
    }
    printf("ext2: inode=%d\r\n", EXT2_ROOT_INODE);
    printf("ext2: typeperm=0x%lx\r\n", Root->TypePerm);
    printf("ext2: sizelow=%d\r\n", Root->SizeLow);
    printf("ext2: creationtime=%d\r\n", Root->CreationTime);
    printf("ext2: lastmodify=%d\r\n", Root->LastModification);
    printf("ext2: gid=%d\r\n", Root->Gid);
    printf("ext2: sectorsinuse=%d\r\n", Root->SectorsInUse);
    printf("ext2: flags=0x%lx\r\n", Root->Flags);
    printf("ext2: dbp0=0x%lx\r\n", Root->Dbp0);
    r = Ext2EnumDirent(Vol, Root->Dbp0);
    MmFree(Root);
}