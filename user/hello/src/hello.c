#include <stdio.h>
#include <ah5kos.h>
#include "../../shared/dwmapi.h"
#include "../../shared/osui/osui.h"
#include "../../shared/osui/btn.h"
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>

KSTATUS DWMCALLBACK WndEventCallback(DwmReEventResponse* info) {
    // printf("event for wnd 0x%lx. mx=%d my=%d mlb=%d mrb=%d mmb=%d\r\n", info->Window, info->MouseX, info->MouseY, info->MouseBtnLeft, info->MouseBtnRight, info->MouseBtnMiddle);
    UiProcessEvent(info);
    return KSUCCESS;
}

KSTATUS UICALLBACK CtrlCallback(UiHandle h, UiEventType e) {
    
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
    UiContext* Ctx = NULL;
    UiInitalize(&Ctx, Window, Fb);
    if (!Ctx) {
        printf("failed to initalize ui context.\r\n");
        goto noui;
    }
    UiSimpleBtn* Btn = UiCreateBtn(Ctx, 128, 128, 64, 64, ARGB(255, 255, 0, 0), ARGB(255, 0, 0, 255));
    if (!Btn) {
        printf("failed to create button.\r\n");
        goto noui;
    }

    noui:
    OsDrawTextAtFb(Fb, "Hello world from program!", 0, 0, ARGB(255, 0, 0, 255), ARGB(255, 255, 255, 255));
    UiDrawCtrls(Ctx);
    DwmApiDrawFinish(Window);
    while (1) {
        OsYield();
    }
    return 0;
}