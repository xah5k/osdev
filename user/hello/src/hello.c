#include <stdio.h>
#include <ah5kos.h>
#include "../../shared/dwmapi.h"
#include "../../shared/osui/osui.h"
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>

static UiContext* gCtx = NULL;

KSTATUS UiProcessEvent(DwmReEventResponse* event) {
    if (!gCtx) return KFAIL;
    if (event->Type == DWMPCK_EVENT_MOUSE) {
        for (int i = 0; i < UI_MAX_CONTROLS; i++) {
            if (gCtx->Controls[i]) {
                if (TESTBOUNDS(event->MouseX, event->MouseY, gCtx->Controls[i]->Pos.x, gCtx->Controls[i]->Pos.y, gCtx->Controls[i]->Width, gCtx->Controls[i]->Height)) {
                    if (event->MouseBtnLeft) {
                        if (!gCtx->OldLmbState) {
                            if (gCtx->Controls[i]->Callback) gCtx->Controls[i]->Callback((UiHandle)gCtx->Controls[i], UI_EVENT_ONCLICK);
                        }
                    }
                }
            }
        }
    }
    gCtx->OldLmbState = event->MouseBtnLeft;
    return KSUCCESS;
}

KSTATUS DWMCALLBACK WndEventCallback(DwmReEventResponse* info) {
    printf("event for wnd 0x%lx. mx=%d my=%d mlb=%d mrb=%d mmb=%d\r\n", info->Window, info->MouseX, info->MouseY, info->MouseBtnLeft, info->MouseBtnRight, info->MouseBtnMiddle);
    UiProcessEvent(info);
    return KSUCCESS;
}

KSTATUS UiInitalize(UiContext** CtxOut, WNDHDL Wnd, Framebuffer* WndFb) {
    UiContext* Ctx = malloc(sizeof(UiContext));
    if (!Ctx) return KOOMERR;
    memset((void*)Ctx, 0, sizeof(UiContext));
    Ctx->Window = Wnd;
    Ctx->WndFb = WndFb;
    Ctx->CtrlCount = 0;
    Ctx->OldLmbState = 0;
    *CtxOut = Ctx;
    return KSUCCESS;
}

// generic function for drawing a control
static KSTATUS UiCtrlDraw(UiHandle CtrlHdl, Framebuffer* Fb) {
    // printf("Drawing Ctrl 0x%lx on fb->ptr 0x%lx\r\n", CtrlHdl, Fb->ptr);
    UiControl* Ctrl = (UiControl*)CtrlHdl;
    DwmApiPutRect(Fb, Ctrl->Pos.x, Ctrl->Pos.y, Ctrl->Width, Ctrl->Height, ARGB(255, 255, 0, 0));
    return KSUCCESS;
}

KSTATUS UiCreateCtrlEx(UiControl** CtrlOut, uint64_t x, uint64_t y, uint64_t w, uint64_t h, UiGenericDraw Draw) {
    UiControl* Ctrl = malloc(sizeof(UiControl));
    if (!Ctrl) return KOOMERR;
    memset((void*)Ctrl, 0, sizeof(UiControl));
    Ctrl->Pos.x = x;
    Ctrl->Pos.y = y;
    Ctrl->Width = w;
    Ctrl->Height = h;
    Ctrl->Draw = Draw;
    Ctrl->Callback = NULL;
    *CtrlOut = Ctrl;
    return KSUCCESS;
}

KSTATUS UiRegisterCtrl(UiContext* Ctx, UiControl* Ctrl, UiCallback Callback) {
    Ctx->Controls[Ctx->CtrlCount] = Ctrl;
    Ctx->Controls[Ctx->CtrlCount]->Callback = Callback;
    Ctx->CtrlCount++;
    // printf("Ctx->Controls[%d] = 0x%lx\r\n", Ctx->CtrlCount-1, Ctx->Controls[Ctx->CtrlCount-1]);
    return KSUCCESS;
}

KSTATUS UiDrawCtrls(UiContext* Ctx) {
    for (int i = 0; i < UI_MAX_CONTROLS; i++) {
        if (Ctx->Controls[i]) {
            // printf("Ctx->Controls[%d] = 0x%lx\r\n", i, Ctx->Controls[i]);
            // printf("Ctx->Controls[%d]->Draw = 0x%lx\r\n", i, Ctx->Controls[i]->Draw);
            if (Ctx->Controls[i]->Draw) Ctx->Controls[i]->Draw((UiHandle)Ctx->Controls[i], Ctx->WndFb);
        }
    }
    return KSUCCESS;
}

KSTATUS UICALLBACK CtrlCallback(UiHandle h, int e) {
    printf("on click event for 0x%lx\r\n", h);
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
    UiControl* Ctrl = NULL;
    UiCreateCtrlEx(&Ctrl, 128, 128, 64, 64, UiCtrlDraw);
    if (!Ctrl) {
        printf("failed to create control.\r\n");
        goto noui;
    }
    gCtx = Ctx;
    UiRegisterCtrl(Ctx, Ctrl, CtrlCallback);
    UiDrawCtrls(Ctx);
    noui:
    OsDrawTextAtFb(Fb, "Hello world from program!", 0, 0, ARGB(255, 0, 0, 255), ARGB(255, 255, 255, 255));
    DwmApiDrawFinish(Window);
    while (1);
    return 0;
}