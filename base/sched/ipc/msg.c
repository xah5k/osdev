#include <sched/ipc/msg.h>
#include <stddef.h>
#include <sched/process.h>
#include <ksyscall.h>
#include <printfwrapper.h>

void KeMessagePushTail(KeMessageObj** Head, KeMessageObj** Tail, uint64_t* MCount, KeMessageObj* Msg, Spinlock* QueueLock) {
    if (!Msg || !Head || !Tail) return;
    uint64_t r = SpnLckAcquireRfl(QueueLock);
    Msg->Next = NULL;
    if (*Tail == NULL) {
        *Head = Msg;
        *Tail = Msg;
    } else {
        (*Tail)->Next = Msg;
        *Tail = Msg;
    }
    if (MCount) (*MCount)++;
    SpnLckReleaseRfl(QueueLock, r);
}

KeMessageObj* KeMessagePopHead(KeMessageObj** Head, KeMessageObj** Tail, uint64_t* MCount, Spinlock* QueueLock) {
    if (Head == NULL) return NULL;
    uint64_t r = SpnLckAcquireRfl(QueueLock);
    if (*Head == NULL) {
        SpnLckReleaseRfl(QueueLock, r);
        return NULL;
    }
    KeMessageObj* Msg = *Head;
    *Head = Msg->Next;
    if (*Head == NULL) {
        *Tail = NULL;
    }
    Msg->Next = NULL;
    if (MCount) (*MCount)--;
    SpnLckReleaseRfl(QueueLock, r);
    return Msg;
}

KSTATUS KeMessageSend(KeMessageObj* Obj) {
    if (!Obj /*|| Obj->FromPid == 0 || Obj->ToPid == 0*/) return KINVALID;
    ProcessCtrlBlk* to = ProcFindByPid(Obj->ToPid, KernelGetInformation());
    if (!to) return KFAIL;
    Obj->Next = NULL;
    KeMessagePushTail(&to->MessageHead, &to->MessageTail, &to->MessageCount, Obj, &to->MessageQueueLock);
    // printf("setting signal %d for pid %d because of message{from=%d, to=%d, len=%lu} msgcount=%d\r\n", KE_SIGNAL_RECEIVEMSG, to->pid, Obj->FromPid, Obj->ToPid, Obj->Length, to->MessageCount);
    ThreadCtrlBlk* current = to->ThreadListHead;
    while (current != NULL) {
        // printf("current@0x%lx\r\n", current);
        current->SigPendingSet |= (1ULL << KE_SIGNAL_RECEIVEMSG);
        current = current->ProcNext;
    }
    return KSUCCESS;
}