#include "fs/tar.h"
#include "kernel.h"
#include <fb.h>
#include <stdint.h>
#include <printfwrapper.h>
#include <memory.h>

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t headersize;
    uint32_t flags;
    uint32_t numglyph;
    uint32_t bytesperglyph;
    uint32_t height;
    uint32_t width;
    uint8_t glyphs;
} __attribute__((packed)) psf2_t;

typedef struct {
    uint16_t magic; // Magic bytes for identification.
    uint8_t fontMode; // PSF font mode.
    uint8_t characterSize; // PSF character size.
} psf1_header_t;

static psf1_header_t* g_fb_font;
static Framebuffer* g_fb_info;

static int glyphcount = 0;

static int gFbConsoleX = 0;
static int gFbConsoleY = 0;
void FbTextInitalize(void* sfn, void* fb) {
    psf1_header_t* font = (psf1_header_t*)sfn;
    if (font->magic != 0x0436) {
        printf("fb: invalid font (not psf1)\r\n");
        return;
    }
    g_fb_font = font;
    g_fb_info = fb;
    if (font->fontMode == 0x01) glyphcount = 512; else glyphcount = 256;
}

void FbPutc(char c) {
    if (c == '\r') {
        gFbConsoleX = 0;
        return;
    }
    if (c == '\n') {
        gFbConsoleX = 0;
        gFbConsoleY += 16;
    } else {
        if (gFbConsoleX + 8 > g_fb_info->width) {
            gFbConsoleX = 0;
            gFbConsoleY += 16;
        }

        if (gFbConsoleY + 16 > g_fb_info->height) {
            gFbConsoleY = 0; 
            memset((void*)g_fb_info->ptr, 0, g_fb_info->size);
        }

        FbPutcAt(c, gFbConsoleX, gFbConsoleY, 0xFFFFFFFF);
        gFbConsoleX += 8;
    }
}

void FbPutcAt(char c, int x, int y, uint32_t color) {
    if (x + 8 > g_fb_info->width || y + 16 > g_fb_info->height) return;
    int char_index = c;
    uint8_t* font = (uint8_t*)g_fb_font;
    uint8_t* glyph =  font + 4 + (char_index * g_fb_font->characterSize);

    for (int _y = 0; _y < g_fb_font->characterSize; _y++) {
        uint32_t* fbrow = (uint32_t*)((uint8_t*)g_fb_info->ptr + (_y + y) * g_fb_info->scanline);

        for (int _x = 0; _x < 8; _x++) {
            if (glyph[_y] & (0x80 >> _x)) {
                fbrow[x + _x] = color;
            }
        }
    }
}