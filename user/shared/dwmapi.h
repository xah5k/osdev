#pragma once
#include <stdint.h>
typedef enum {
    DWMPCK_CRWIN, // creates a window and returns the fbinfo struct for it
    DWMPCK_REFBINFO,
    DWMPCK_DRAW
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