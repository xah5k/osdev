#pragma once
#include <stdint.h>
typedef enum {
    DWMPCK_CRWIN, // creates a window and returns the fbinfo struct for it
    DWMPCK_REFBINFO,
    DWMPCK_DRAW
} DwmPckType;

typedef struct {
    DwmPckType Type;
    uint64_t CrWinWidth;
    uint64_t CrWinHeight;
} DwmPacket;
typedef void* WNDHDL;
typedef struct {
    Framebuffer fbinf;
    WNDHDL WindowHandle;
} DwmCrWinResponse;