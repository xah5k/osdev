#include <stdio.h>
#include <ah5kos.h>
#include "../../shared/dwmapi.h"
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>

KSTATUS DWMCALLBACK WndEventCallback(DwmReEventResponse* info) {
    printf("event for wnd 0x%lx. mx=%d my=%d mlb=%d mrb=%d mmb=%d\r\n", info->Window, info->MouseX, info->MouseY, info->MouseBtnLeft, info->MouseBtnRight, info->MouseBtnMiddle);
    return KSUCCESS;
}

int main(int argc, const char* argv[]) {
    DwmApiInitalize();
    WNDHDL Window = NULL;
    Framebuffer* Fb = malloc(sizeof(Framebuffer));
    DwmApiCreateWin(256, 256, 256, 256, DWMPCK_CRWIN_WINDOWED, &Window, Fb);
    if (!Window) {
        perror("window creation fail");
        return -1;
    }
    DwmApiChangeName(Window, "Test program");
    printf("created window.\r\n");
    DwmApiPutRect(Fb, 64, 64, 10, 10, ARGB(255, 255, 0, 0));
    OsPutcAtFb(Fb, 'A', 128, 128, ARGB(255, 255, 0, 0), ARGB(255, 255, 255, 255));
    OsDrawTextAtFb(Fb, "Hello world from program!", 0, 0, ARGB(255, 0, 0, 255), ARGB(255, 255, 255, 255));
    DwmApiDrawFinish(Window);
    while (1);
    return 0;
}