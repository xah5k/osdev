
#include <stdio.h>
#include <stdint.h>
#include <signal.h>
extern uint64_t dosyscall(uint64_t sys_num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4,  uint64_t arg5);

static volatile int caught = 0;
int handler(int sigidx) {
    printf("caught signal %d\r\n", sigidx);
    printf("ok bye.\r\n");
    caught = 1;
}
int main() {
    printf("registering handler for sigint.\r\n");
    dosyscall(35, SIGINT, (uint64_t)handler, 0, 0, 0);
    int x = 0;
    while (!caught) {
        x++;
    }
    printf("caught sigint.\r\n");
    return 0;
}