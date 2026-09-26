#pragma once
#include "../../shared/dwmapi.h"
#ifdef __cplusplus
extern "C" {
#endif

struct UiControl;
#define UICALLBACK

#define UI_MAX_CONTROLS 256 // id be worried if we need any more than that
typedef struct UiControl* UiHandle;
typedef enum {
    UI_EVENT_ONCLICK,
} UiEventType;

typedef KSTATUS(*UiCallback)(UiHandle h, UiEventType e);
typedef KSTATUS(*UiGenericDraw)(UiHandle h, Framebuffer* f);
typedef struct {
    struct UiControl* Controls[UI_MAX_CONTROLS];
    uint8_t CtrlCount;
    WNDHDL Window;
    Framebuffer* WndFb;
    int OldLmbState;
} UiContext;

typedef struct UiControl {
    Point Pos;
    uint64_t Width;
    uint64_t Height;
    UiCallback Callback;
    UiGenericDraw Draw;
    UiContext* Owner;
} UiControl;


#define TESTBOUNDS(x1, y2, x, y, w, h) (x1 >= x && x1 < (x + w) && y2 >= y && y2 < (y + h))

KSTATUS UiInitalize(UiContext** CtxOut, WNDHDL Wnd, Framebuffer* WndFb);
KSTATUS UiCreateCtrlEx(UiControl** CtrlOut, uint64_t x, uint64_t y, uint64_t w, uint64_t h, UiGenericDraw Draw);
KSTATUS UiRegisterCtrl(UiContext* Ctx, UiControl* Ctrl, UiCallback Callback);
KSTATUS UiDrawCtrls(UiContext* Ctx);
KSTATUS UiProcessEvent(DwmReEventResponse* event);

#ifdef __cplusplus
}
#endif