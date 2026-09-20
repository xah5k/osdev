#include <ah5kos.h>
#include "../../shared/dwmapi.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static volatile bool gReFbInfRecv = false;
static volatile uint64_t mlength = sizeof(DwmPacket) + sizeof(DwmCrWinResponse);
static Framebuffer* gFbInfo = NULL;
static WNDHDL Window = NULL;

void SignalReceive(int SigIdx) {
    OsMessage* m = OsMsgGet(NULL, mlength);
    if (!m || m < 0x1024) return;
    DwmPacket* p = (DwmPacket*)((uint64_t)m + sizeof(OsMessage));
    switch (p->Type) {
        case DWMPCK_REFBINFO: {
            DwmCrWinResponse* resp = (DwmCrWinResponse*)((uint64_t)p + sizeof(DwmPacket));
            Framebuffer* finfo = (Framebuffer*)&resp->fbinf;
            Window = resp->WindowHandle;
            if (!gReFbInfRecv) {
                // mlength += finfo->size;
                gFbInfo = malloc(sizeof(Framebuffer));
                gReFbInfRecv = true;
            }
            if (gFbInfo) memcpy((void*)gFbInfo, finfo, sizeof(Framebuffer));
            break;
        }
        default: {
            // printf("dropping dwm packet. invalid type %d.\r\n", p->Type);
            break;
        }
    }
    free(m);
}

KSTATUS DwmApiCreateWin(uint64_t Width, uint64_t Height, DwmCrWinOpt Opt, WNDHDL* WndOut, Framebuffer* FbOut) {
    OsMessage* m = malloc(sizeof(OsMessage) + sizeof(DwmPacket));
    m->FromPid = getpid();
    m->ToPid = 1; // dwm should be pid 1.
    m->Length = sizeof(DwmPacket);
    DwmPacket* p = (DwmPacket*)((uint64_t)m + sizeof(OsMessage));
    p->CrWinWidth = Width;
    p->CrWinHeight = Height;
    p->CrWinOpt = Opt;
    p->Type = DWMPCK_CRWIN;
    KSTATUS s;
    s = OsMsgSend(m);
    while (!gReFbInfRecv);
    gReFbInfRecv = false;
    *WndOut = Window;
    memcpy((void*)FbOut, gFbInfo, sizeof(Framebuffer));
    free(gFbInfo);
    Window = NULL;
    gFbInfo = NULL;
    free(m);
    return s;
}

KSTATUS DwmApiDrawFinish(WNDHDL w) {
    OsMessage* m = malloc(sizeof(OsMessage) + sizeof(DwmPacket) + sizeof(WNDHDL));
    m->FromPid = getpid();
    m->ToPid = 1; // dwm should be pid 1.
    m->Length = sizeof(DwmPacket) + sizeof(WNDHDL);
    DwmPacket* p = (DwmPacket*)((uint64_t)m + sizeof(OsMessage));
    p->CrWinWidth = 0;
    p->CrWinHeight = 0;
    p->CrWinOpt = DWMPCK_CRWIN_DEFAULT;
    p->Type = DWMPCK_DRAW;
    WNDHDL* pwhdl = (WNDHDL*)((uint64_t)p + sizeof(DwmPacket));
    *pwhdl = w;
    KSTATUS s;
    s = OsMsgSend(m);
    free(m);
    return s;
}


#define FBOFF(x, y, fbinfo) ((y) * (fbinfo)->scanline + (x) * ((fbinfo)->bpp / 8))

void PutPixel(Framebuffer* fb, int64_t x, int64_t y, uint32_t color) {
    uint32_t offset = FBOFF(x, y, fb);
    *(uint32_t*)((uint8_t*)fb->ptr + offset) = color;
}

void PutRect(Framebuffer* fb, int64_t x, int64_t y, uint64_t w, uint64_t h, uint32_t color) {
    if (x < 0 || y < 0) return;
    if ((uint64_t)x + w > fb->width) return;
    if ((uint64_t)y + h > fb->height) return;
    uint32_t offset = FBOFF(x, y, fb);
    uint8_t* RowPtr = (uint8_t*)fb->ptr + offset;
    for (uint64_t j = 0; j < h; j++) {
        uint32_t* px = (uint32_t*)RowPtr;
        for (uint64_t i = 0; i < w; i++) {
            px[i] = color;
        }
        RowPtr += fb->scanline;
    }
}

KSTATUS DwmApiInitalize() {
    OsSleep(2); // hack
    KSTATUS s;
    uint64_t r = dosyscall(36, SIGNAL_RECEIVEMSG, (uint64_t)SignalReceive, 0, 0, 0);
    return KSUCCESS;
}