#include "../../shared/osui/btn.h"
#include <ah5kos.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

static KSTATUS UiCtrlDraw(UiHandle CtrlHdl, Framebuffer* Fb) {
    UiSimpleBtn* Btn = (UiSimpleBtn*)CtrlHdl;
    DwmApiPutRect(Fb, Btn->Ctrl.Pos.x, Btn->Ctrl.Pos.y, Btn->Ctrl.Width, Btn->Ctrl.Height, Btn->ColourCurrent);
    return KSUCCESS;
}

static KSTATUS UICALLBACK CtrlCallback(UiHandle h, UiEventType e) {
    if (e == UI_EVENT_ONCLICK) {
        int s = time(NULL);
        printf("current = %d\r\n", s);
        puts("clicked on button.\r\n");
        UiSimpleBtn* Btn = (UiSimpleBtn*)h;
        Btn->ColourCurrent = Btn->ColourOnClick;
        UiDrawCtrls(Btn->Ctrl.Owner);
        DwmApiDrawFinish(Btn->Ctrl.Owner->Window);
        Btn->ColourCurrent = Btn->ColourDefault;
        UiDrawCtrls(Btn->Ctrl.Owner);
        DwmApiDrawFinish(Btn->Ctrl.Owner->Window);
    }
    return KSUCCESS;
}

UiSimpleBtn* UiCreateBtn(UiContext* ctx, uint64_t x, uint64_t y, uint64_t w, uint64_t h, uint32_t colour, uint32_t colouronclk) {
    UiControl* tmp = NULL;
    KSTATUS s = UiCreateCtrlEx(&tmp, x, y, w, h, UiCtrlDraw);
    assert(tmp != NULL);
    assert(s == KSUCCESS);
    UiSimpleBtn* btn = malloc(sizeof(UiSimpleBtn));
    memcpy(btn, tmp, sizeof(UiControl));
    free(tmp);
    btn->ColourDefault = colour;
    btn->ColourOnClick = colouronclk;
    btn->ColourCurrent = colour;
    btn->UserCallback = NULL;
    UiRegisterCtrl(ctx, (UiControl*)btn, CtrlCallback);
    return btn;
}
