#include <fs/io/pipe.h>
#include <fs/vfs.h>
#include <sched/process.h>
#include <mm/heap.h>
// allocates a iopipeobj and reserves 1 handle for reading and 1 handle for writing in the passed process
IoPipeObj* IoCreatePipe(struct ProcessCtrlBlk* proc) {
    if (!proc) return NULL;
    if (!proc->FileHandleTable) return NULL;
    if (proc->nextfh >= VFS_MAX_ALLOWED_OPEN_HANDLES) return NULL;
    if ((proc->nextfh + 2) >= VFS_MAX_ALLOWED_OPEN_HANDLES) return NULL;
    int readhandle = proc->nextfh;
    proc->nextfh++;
    int writehandle = proc->nextfh;
    proc->nextfh++;
    IoPipeObj* pipe = MmAllocate(sizeof(IoPipeObj));
    pipe->ReadHandle = (uint64_t)readhandle;
    pipe->WriteHandle = (uint64_t)writehandle;
    pipe->Count = 0;
    pipe->ReadPos = 0;
    pipe->WritePos = 0;
    pipe->RefCount = 2;
    proc->FileHandleTable[readhandle].Flag = VFS_OFD_FLAG_PIPE;
    proc->FileHandleTable[writehandle].Flag = VFS_OFD_FLAG_PIPE;
    proc->FileHandleTable[readhandle].PipeEntry = pipe;
    proc->FileHandleTable[writehandle].PipeEntry = pipe;
    return pipe;
}