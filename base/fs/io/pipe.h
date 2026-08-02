#pragma once
#include <stdint.h>
#define IO_PIPE_BUF_SZ 1024
typedef struct {
    uint8_t Buffer[IO_PIPE_BUF_SZ];
    uint64_t WritePos;
    uint64_t ReadPos;
    uint64_t Count;
    uint64_t ReadHandle;
    uint64_t WriteHandle;
    uint32_t RefCount;
} IoPipeObj;

struct ProcessCtrlBlk;
IoPipeObj* IoCreatePipe(struct ProcessCtrlBlk* proc);