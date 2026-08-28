
#include <stdio.h>
#include <stdint.h>
#include <ah5kos.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

extern uint64_t dosyscall(uint64_t sys_num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4,  uint64_t arg5);

int main(int argc, const char* argv[]) {
    KSTATUS s;
    printf("testing fb.\r\n");
    Framebuffer* fb = OsCreateFb(128, 128);
    printf("fb->width=%lu fb->height=%lu\r\n", fb->width, fb->height);
    memset((void*)fb->ptr, 0xFF, fb->size);
    printf("draw test fb at 128,128\r\n");
    Framebuffer* gfb = malloc(sizeof(Framebuffer));
    s = OsGetFbInfo(gfb);
    printf("KSTATUS 0x%lx\r\n", s);
    if (!gfb) {
        printf("failed to get global fb.\r\n");
        goto exit;
    }
    s = OsDrawFb(gfb, fb, 128, 128);
    printf("KSTATUS 0x%lx\r\n", s);
    exit:
    s = OsFreeFb(fb);
    return 0;
}