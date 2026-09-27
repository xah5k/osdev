#pragma once
#include <stdint.h>
#include <ah5kos.h>

typedef enum {
    DWMPCK_CRWIN, // creates a window and returns the fbinfo struct for it
    DWMPCK_REFBINFO,
    DWMPCK_DRAW,
    DWMPCK_CHNAME,
    DWMPCK_REEVENT,
} DwmPckType;

typedef enum {
    DWMPCK_CRWIN_WINDOWED,
    DWMPCK_CRWIN_BORDERLESS, // dwm doesnt draw titlebar and stuff
    DWMPCK_CRWIN_DEFAULT = DWMPCK_CRWIN_WINDOWED,
} DwmCrWinOpt;

typedef enum {
    DWMPCK_EVENT_MOUSE,
    DWMPCK_EVENT_KBD,
} DwmReEventType;

typedef struct {
    DwmPckType Type;
    DwmCrWinOpt CrWinOpt;
    uint64_t CrWinWidth;
    uint64_t CrWinHeight;
    uint64_t CrWinX;
    uint64_t CrWinY;
} DwmPacket;

typedef void* WNDHDL;
typedef struct {
    Framebuffer fbinf;
    WNDHDL WindowHandle;
} DwmCrWinResponse;

typedef struct {
    WNDHDL Window;
    DwmReEventType Type;
    uint32_t MouseX;
    uint32_t MouseY;
    uint32_t MouseBtnLeft;
    uint32_t MouseBtnRight;
    uint32_t MouseBtnMiddle;
    char KbdChar;
} DwmReEventResponse;

// just a very useful macro
#define ARGB(a, r, g, b) (a << 24) | (r << 16) | (g << 8) | b

// descriptive
#define DWMCALLBACK

KSTATUS DwmApiInitalize();
void DwmApiPutRect(Framebuffer* fb, int64_t x, int64_t y, uint64_t w, uint64_t h, uint32_t color);
void DwmApiPutPixel(Framebuffer* fb, int64_t x, int64_t y, uint32_t color);
KSTATUS DwmApiDrawFinish(WNDHDL w);
KSTATUS DwmApiCreateWin(uint64_t Width, uint64_t Height, uint64_t X, uint64_t Y, DwmCrWinOpt Opt, WNDHDL* WndOut, Framebuffer* FbOut);
KSTATUS DwmApiChangeName(WNDHDL w, const char* Name);