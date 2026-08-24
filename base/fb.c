#include "fs/tar.h"
#include "kernel.h"
#include <fb.h>
#include <stdint.h>
#include <printfwrapper.h>
#include <memory.h>
#include <kedriver.h>
#include <mm/heap.h>

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
static uint64_t gFbConsoleBg = 0x00000000;

void FbTextInitalize(void* sfn, void* fb) {
    psf1_header_t* font = (psf1_header_t*)sfn;
    if (font->magic != 0x0436) {
        printf("fb: invalid font (not psf1)\r\n");
        return;
    }
    g_fb_font = font;
    g_fb_info = fb;
    printf("fb: framebuffer info: \r\n");
    printf("fb: screen %dx%d\r\n", g_fb_info->width, g_fb_info->height);
    printf("fb: fb ptr @ 0x%lx\r\n", g_fb_info->ptr);
    printf("fb: fb scanline %d\r\n", g_fb_info->scanline);
    memset((void*)g_fb_info->ptr, 0xFF, g_fb_info->scanline * 100);
    if (font->fontMode == 0x01) glyphcount = 512; else glyphcount = 256;
}
KE_EXPORT_SYMBOL(FbTextInitalize);

void FbPutc(char c) {
    if (c == 0) return;
    if (c == '\r') {
        gFbConsoleX = 0;
        return;
    }
    if (c == '\b') {
        gFbConsoleX -= 8;
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

        FbPutcAt(c, gFbConsoleX, gFbConsoleY, 0xFFFFFFFF, gFbConsoleBg);
        gFbConsoleX += 8;
    }
}
KE_EXPORT_SYMBOL(FbPutc);
void FbPutcAt(char c, int x, int y, uint32_t fg_color, uint32_t bg_color) {
    if (x + 8 > g_fb_info->width || y + g_fb_font->characterSize > g_fb_info->height) return;
    
    uint8_t char_index = (uint8_t)c; 
    if (char_index >= glyphcount) return;

    uint8_t* glyph = ((uint8_t*)g_fb_font) + 4 + (char_index * g_fb_font->characterSize);

    for (int _y = 0; _y < g_fb_font->characterSize; _y++) {
        uint32_t* fbrow = (uint32_t*)((uint8_t*)g_fb_info->ptr + (_y + y) * g_fb_info->scanline);

        for (int _x = 0; _x < 8; _x++) {
            if (glyph[_y] & (0x80 >> _x)) {
                fbrow[x + _x] = fg_color;
            } else {
                fbrow[x + _x] = bg_color;
            }
        }
    }
}
KE_EXPORT_SYMBOL(FbPutcAt);

void FbClear() {
    gFbConsoleX = 0;
    gFbConsoleY = 0;
    memset((void*)g_fb_info->ptr, 0, g_fb_info->size);
}

Framebuffer* FbCreate(uint32_t width, uint32_t height) {
    Framebuffer* fbinfo = MmAllocate(sizeof(Framebuffer));
    fbinfo->width = width;
    fbinfo->height = height;
    fbinfo->scanline = g_fb_info->scanline;
    fbinfo->size = height * fbinfo->scanline;
    fbinfo->ptr = (uint64_t)MmAllocate(fbinfo->size);
    memset((void*)fbinfo->ptr, 0, fbinfo->size);
    return fbinfo;
}

KSTATUS FbFree(Framebuffer* fb) {
    if (!fb) return KINVALID;
    if (fb->ptr == 0) return KINVALID;
    MmFree((void*)fb->ptr);
    MmFree(fb);
    return KSUCCESS;
}

KSTATUS FbDraw(Framebuffer* destfb, Framebuffer* srcfb, int x, int y) {
    if (x < 0 || y < 0) return KINVALID;
    if (x + srcfb->width > destfb->width) return KINVALID;
    if (y + srcfb->height > destfb->height) return KINVALID;

    uint8_t* FbPtr = (uint8_t*)destfb->ptr + FBOFF(x, y, destfb);
    uint8_t* FbSrcPtr = (uint8_t*)srcfb->ptr;
    uint32_t RowBytes = srcfb->width * (destfb->bpp / 8);

    for (uint32_t i = 0; i < srcfb->height; i++) {
        memcpy(FbPtr, FbSrcPtr, RowBytes);
        FbSrcPtr += srcfb->scanline;
        FbPtr += destfb->scanline;
    }
    return KSUCCESS;
}
void FbPutcAtIn(Framebuffer* fb, char c, int x, int y, uint32_t fg_color, uint32_t bg_color) {
    if (x + 8 > fb->width || y + g_fb_font->characterSize > fb->height) return;
    
    uint8_t char_index = (uint8_t)c; 
    if (char_index >= glyphcount) return;

    uint8_t* glyph = ((uint8_t*)g_fb_font) + 4 + (char_index * g_fb_font->characterSize);

    for (int _y = 0; _y < g_fb_font->characterSize; _y++) {
        uint32_t* fbrow = (uint32_t*)((uint8_t*)fb->ptr + (_y + y) * fb->scanline);

        for (int _x = 0; _x < 8; _x++) {
            if (glyph[_y] & (0x80 >> _x)) {
                fbrow[x + _x] = fg_color;
            } else {
                fbrow[x + _x] = bg_color;
            }
        }
    }
}