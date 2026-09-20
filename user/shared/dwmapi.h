#pragma once
#include <stdint.h>
typedef enum {
    DWMPCK_CRWIN, // creates a window and returns the fbinfo struct for it
    DWMPCK_REFBINFO,
    DWMPCK_DRAW,
    DWMPCK_CHNAME,
} DwmPckType;

typedef enum {
    DWMPCK_CRWIN_WINDOWED,
    DWMPCK_CRWIN_BORDERLESS, // dwm doesnt draw titlebar and stuff
    DWMPCK_CRWIN_DEFAULT = DWMPCK_CRWIN_WINDOWED,
} DwmCrWinOpt;

typedef struct {
    DwmPckType Type;
    DwmCrWinOpt CrWinOpt;
    uint64_t CrWinWidth;
    uint64_t CrWinHeight;
} DwmPacket;

typedef void* WNDHDL;
typedef struct {
    Framebuffer fbinf;
    WNDHDL WindowHandle;
} DwmCrWinResponse;

// just a very useful macro
#define ARGB(a, r, g, b) (a << 24) | (r << 16) | (g << 8) | b

KSTATUS DwmApiInitalize();
void PutRect(Framebuffer* fb, int64_t x, int64_t y, uint64_t w, uint64_t h, uint32_t color);
void PutPixel(Framebuffer* fb, int64_t x, int64_t y, uint32_t color);
KSTATUS DwmApiDrawFinish(WNDHDL w);
KSTATUS DwmApiCreateWin(uint64_t Width, uint64_t Height, DwmCrWinOpt Opt, WNDHDL* WndOut, Framebuffer* FbOut);