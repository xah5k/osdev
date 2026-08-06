#include <fs/ext2.h>
#include <printfwrapper.h>
void Ext2SbDump(void* base) {
    Ext2Superblock* Sb = (Ext2Superblock*)((uint64_t)base + 1024);
    if (Sb->Magic != 0xEF53) {
        printf("ext2: failed to verify superblock magic.\r\n");
        printf("ext2: expected 0xEF53 but got 0x%lx instead.\r\n", Sb->Magic);
        return;
    }
    printf("ext2: InodeCount=%d\r\n", Sb->InodeCount);
    printf("ext2: BlockCount=%d\r\n", Sb->BlockCount);
    printf("ext2: RsvBlockCount=%d\r\n", Sb->RsvBlockCount);
    printf("ext2: FreeBlocksCount=%d\r\n", Sb->FreeBlocksCount);
    printf("ext2: FreeInodeCount=%d\r\n", Sb->FreeInodeCount);
    printf("ext2: FirstDataBlk=%d\r\n", Sb->FirstDataBlk);
    printf("ext2: CreatorOs=%d\r\n", Sb->CreatorOs);
}