#pragma once
#include <fs/vfs.h>
#include <kernel.h>

typedef struct {
    uint64_t StdinBufferFl; // if set stdin is only flushed on a newline if not then normal behaviour
    uint64_t StdoutBufferFl; // if set stdout is only flushed (well printed) on a newline.
} KeTtyInfo;

typedef struct {
    int GlobalStdinHdl;
    int GlobalStdoutHdl;
    int GlobalStderrHdl;
    KeTtyInfo* TtyInfo;
    char StdinBuf[128];
    char StdoutBuf[1024];
} KeTerminalObj;

KeTerminalObj* TtyCreateObj(int Stdin, int Stdout, int Stderr);
int TtyWrite(KeTerminalObj* TtyObj, const char* output, uint64_t len);
uint64_t TtyRead(KeTerminalObj* TtyObj, char* inbuf, uint64_t len);