#pragma once
#include <stdint.h>
#include <kernel.h>
#define ARGB(a, r, g, b) (a << 24) | (r << 16) | (g << 8) | b
#define FBOFF(x, y, fbinfo) (y * fbinfo->scanline) + (x * fbinfo->bpp)

void FbTextInitalize(void* sfn, void* fb);
void FbPutc(char c);
void FbPutcAt(char c, int x, int y, uint32_t fg_color, uint32_t bg_color);
void FbClear();
Framebuffer* FbCreate(uint32_t width, uint32_t height, uint64_t);
KSTATUS FbFree(Framebuffer* fb, uint64_t);
KSTATUS FbDraw(Framebuffer* destfb, Framebuffer* srcfb, int x, int y);
void FbPutcAtIn(Framebuffer* fb, char c, int x, int y, uint32_t fg_color, uint32_t bg_color);