
#include <stdio.h>
#include <stdint.h>
#include <ah5kos.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
extern uint64_t dosyscall(uint64_t sys_num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4,  uint64_t arg5);

int main(int argc, const char* argv[]) {
    int h = open("krnlfs:/Devices/ps2mouse", 0);
    if (h < 0) {
        printf("failed to get handle for mouse.\r\n");
        return 1;
    } else {
        while (1) {
            KeDevMousePacket* packet = malloc(sizeof(KeDevMousePacket));
            int c = read(h, packet, sizeof(KeDevMousePacket));
            if (c <= 0) {
                continue;
            }
            if (packet) {
                printf("%s: mouse dx=%d dy=%d l=%d m=%d r=%d\r\n", argv[0], packet->RawPos.x, packet->RawPos.y, packet->LeftClickPress, packet->MiddleClickPress, packet->RightClickPress);
            }
            free(packet);
            asm ("nop");
            asm ("nop");
            asm ("nop");
            asm ("nop");
            asm ("nop");
            asm ("nop");
            asm ("nop");
            asm ("nop");
            asm ("nop");
            
        }
    }
    return 0;
}