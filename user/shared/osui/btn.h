#pragma once
#include "osui.h"

typedef struct {
    UiControl Ctrl;
    uint32_t ColourDefault;
    uint32_t ColourOnClick;
    uint32_t ColourCurrent;
    UiCallback UserCallback; // user defined
} UiSimpleBtn;

UiSimpleBtn* UiCreateBtn(UiContext* ctx, uint64_t x, uint64_t y, uint64_t w, uint64_t h, uint32_t colour, uint32_t colouronclk);