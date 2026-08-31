#pragma once
#include <ah5kos.h>
#include <stdbool.h>

class Dwm {
    public:
    Dwm(const char* MousePath);
    ~Dwm();
    KSTATUS GetStatus();
    private:
    KSTATUS HandleMouse(int handle);
    KSTATUS Draw();
    int64_t CursorPosX = 0;
    int64_t CursorPosY = 0;
    int64_t OldCursorPosX = 0;
    int64_t OldCursorPosY = 0;
    Framebuffer* FrontBuffer;
    Framebuffer* CursorBuffer;
    Framebuffer* GlobalBuffer;
    KSTATUS Status = KSUCCESS;
    bool DrawFullBuffer = false;
    void PutPixel(Framebuffer* fb, int64_t x, int64_t y, uint32_t color);
    void PutRect(Framebuffer* fb, int64_t x, int64_t y, uint64_t w, uint64_t h, uint32_t color);
};