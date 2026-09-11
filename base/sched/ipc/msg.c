#include <sched/ipc/msg.h>
#include <stddef.h>
#include <sched/process.h>
#include <ksyscall.h>

void KeMessagePushTail(KeMessageObj** Head, KeMessageObj** Tail, uint64_t* MCount, KeMessageObj* Msg) {
    if (!Msg || !Head || !Tail) return;
    Msg->Next = NULL;
    if (*Tail == NULL) {
        *Head = Msg;
        *Tail = Msg;
    } else {
        (*Tail)->Next = Msg;
        *Tail = Msg;
    }
    if (MCount) *MCount++;
}

KeMessageObj* KeMessagePopHead(KeMessageObj** Head, KeMessageObj** Tail, uint64_t* MCount) {
    if (Head == NULL || *Head == NULL) return NULL;

    KeMessageObj* Msg = *Head;
    *Head = Msg->Next;

    if (*Head == NULL) {
        *Tail = NULL;
    }

    Msg->Next = NULL;
    if (MCount) *MCount--;
    return Msg;
}

KSTATUS KeMessageSend(KeMessageObj* Obj) {
    if (!Obj /*|| Obj->FromPid == 0 || Obj->ToPid == 0*/) return KINVALID;
    ProcessCtrlBlk* to = ProcFindByPid(Obj->ToPid, KernelGetInformation());
    if (!to) return KFAIL;
    KeMessagePushTail(&to->MessageHead, &to->MessageTail, &to->MessageCount, Obj);
    ThreadCtrlBlk* current = to->ThreadListHead;
    while (current != NULL) {
        current->SigPendingSet |= (1ULL << KE_SIGNAL_RECEIVEMSG);
        current = current->ProcNext;
    }
    return KSUCCESS;
}