#include <ttyobj.h>
#include <mm/heap.h>
#include <printfwrapper.h>
#include <util/util.h>
#include <memory.h>
#include <util/kbdtransl.h>
KeTerminalObj* TtyCreateObj(int Stdin, int Stdout, int Stderr) {
    KeTerminalObj* TtyObj = MmAllocate(sizeof(KeTerminalObj));
    TtyObj->GlobalStdinHdl = Stdin;
    TtyObj->GlobalStdoutHdl = Stdout;
    TtyObj->GlobalStderrHdl = Stderr;
    TtyObj->TtyInfo = MmAllocate(sizeof(KeTtyInfo));
    TtyObj->TtyInfo->StdinBufferFl = 1;
    TtyObj->TtyInfo->StdoutBufferFl = 0;
    return TtyObj;
}

int TtyWrite(KeTerminalObj* TtyObj, const char* output, uint64_t len) {
    if (TtyObj->TtyInfo->StdoutBufferFl) {
        for (uint64_t i = 0; i < len; i++) {
            if (TtyObj->StdoutBuf[i] == '\n') {
                for (uint64_t j = 0; j < len; j++) _putchar(TtyObj->StdoutBuf[j]);
            }
            TtyObj->StdoutBuf[i] = output[i];
        }
        return len;
    } else {
        for (uint64_t i = 0; i < len; i++) _putchar(output[i]);
        return len;
    }
    return -1;
}

uint64_t TtyRead(KeTerminalObj* TtyObj, char* inbuf, uint64_t len) {
    if (TtyObj->TtyInfo->StdinBufferFl) {
        for (uint64_t i = 0; i < len; i++) {
            if (TtyObj->StdinBuf[i] == '\n') {
                memcpy((void*)inbuf, TtyObj->StdinBuf, i);
                return i;
            }
            TtyObj->StdinBuf[i] = KbdTranslGetc();
        }
        return len;
    } else {
        char array[len];
        for (uint64_t i = 0; i < len; i++) {
            array[i] = KbdTranslGetc();
        }
        return len;
    }
    return -1;
}