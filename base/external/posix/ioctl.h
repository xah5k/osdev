#pragma once
#include <stdint.h>
typedef struct {
    uint16_t ws_row;
    uint16_t ws_col;
    uint16_t ws_xpixel;
    uint16_t ws_ypixel;
} unixwinsize;

#define TIOCGWINSZ 0x5413