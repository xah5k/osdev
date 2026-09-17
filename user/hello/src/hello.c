#include <stdio.h>
#include <ah5kos.h>
#include "../../shared/dwmapi.h"
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>

bool gReFbInfRecv = false;
bool gGetFbRecv = false;

volatile uint64_t mlength = sizeof(DwmPacket) + sizeof(DwmCrWinResponse);
Framebuffer* gFbInfo = NULL;
void* gFbPtr = NULL;
WNDHDL Window = NULL;
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
                mlength += finfo->size;
                gFbInfo = malloc(sizeof(Framebuffer));
                gReFbInfRecv = true;
            }
            if (gFbInfo) memcpy((void*)gFbInfo, finfo, sizeof(Framebuffer));
            break;
        }
        default: {
            printf("dropping dwm packet. invalid type %d.\r\n", p->Type);
            break;
        }
    }
    free(m);
}

KSTATUS DwmApiCreateWin(uint64_t Width, uint64_t Height) {
    OsMessage* m = malloc(sizeof(OsMessage) + sizeof(DwmPacket));
    m->FromPid = getpid();
    m->ToPid = 1; // dwm should be pid 1.
    m->Length = sizeof(DwmPacket);
    DwmPacket* p = (DwmPacket*)((uint64_t)m + sizeof(OsMessage));
    p->CrWinWidth = Width;
    p->CrWinHeight = Height;
    p->Type = DWMPCK_CRWIN;
    KSTATUS s;
    s = OsMsgSend(m);
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
    p->Type = DWMPCK_DRAW;
    WNDHDL* pwhdl = (WNDHDL*)((uint64_t)p + sizeof(DwmPacket));
    *pwhdl = w;
    KSTATUS s;
    s = OsMsgSend(m);
    free(m);
    return s;
}

#define ARGB(a, r, g, b) (a << 24) | (r << 16) | (g << 8) | b
#define FBOFF(x, y, fbinfo) ((y) * (fbinfo)->scanline + (x) * ((fbinfo)->bpp / 8))

void PutPixel(uint64_t fb, int64_t x, int64_t y, uint32_t color) {
    uint32_t offset = FBOFF(x, y, gFbInfo);
    *(uint32_t*)((uint8_t*)fb + offset) = color;
}

void PutRect(uint64_t fb, int64_t x, int64_t y, uint64_t w, uint64_t h, uint32_t color) {
    if (x < 0 || y < 0) return;
    if ((uint64_t)x + w > gFbInfo->width) return;
    if ((uint64_t)y + h > gFbInfo->height) return;
    uint32_t offset = FBOFF(x, y, gFbInfo);
    uint8_t* RowPtr = (uint8_t*)fb + offset;
    for (uint64_t j = 0; j < h; j++) {
        uint32_t* px = (uint32_t*)RowPtr;
        for (uint64_t i = 0; i < w; i++) {
            px[i] = color;
        }
        RowPtr += gFbInfo->scanline;
    }
}

int main(int argc, const char* argv[]) {
    OsSleep(2); // wait for dwm to initalize properly
    KSTATUS s;
    uint64_t r = dosyscall(36, SIGNAL_RECEIVEMSG, (uint64_t)SignalReceive, 0, 0, 0);
    printf("creating 128x128 window.\r\n");
    s = DwmApiCreateWin(128, 128);
    while (!gReFbInfRecv); // as ugly as this is we need it for synchronizing the server and client
    printf("after. KSTATUS 0x%x\r\n", s);
    PutRect(gFbInfo->ptr, 64, 64, 10, 10, ARGB(255, 255, 0, 0));
    DwmApiDrawFinish(Window);
    while (1);
    return 0;
}