#pragma once
#include <stdint.h>
#define ARGB(a, r, g, b) (a << 24) | (r << 16) | (g << 8) | b
void FbTextInitalize(void* sfn, void* fb);
void FbPutc(char c);
void FbPutcAt(char c, int x, int y, uint32_t fg_color, uint32_t bg_color);
void FbClear();