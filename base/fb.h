#pragma once
#include <stdint.h>
void FbTextInitalize(void* sfn, void* fb);
void FbPutc(char c);
void FbPutcAt(char c, int x, int y, uint32_t fg_color, uint32_t bg_color);
void FbClear();