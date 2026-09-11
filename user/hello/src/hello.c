
#include <stdio.h>
#include <stdint.h>
#include <ah5kos.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
static volatile int caught = 0;
extern uint64_t dosyscall(uint64_t sys_num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4,  uint64_t arg5);

int handler(int sigidx) {
    uint64_t len = 0;
    OsMessage* m = OsMsgGet(&len, 128);
    if (!m) {
        printf("invalid message. (m@0x%lx??)\r\n", m);
        caught = 1;
        return -1;
    }
    printf("message with len %d\r\n", len);
    printf("message from pid %d -> pid %d.\r\n", m->FromPid, m->ToPid);
    const char* content = (const char*)((uint64_t)m + sizeof(OsMessage));
    for (int i = 0; i < m->Length; i++) putchar(content[i]);
    free(m);
    caught = 1;
    return 0;
}

int main(int argc, const char* argv[]) {
    dosyscall(36, 45, (uint64_t)handler, 0, 0, 0);
    while (!caught);
    return 0;
}