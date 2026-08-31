#pragma once
#include <ah5kos.h>
#include <stdbool.h>
#include <list>
class Window;
class Dwm {
    public:
    Dwm();
    Dwm(const char* MousePath);
    ~Dwm();
    void Start();
    KSTATUS GetStatus();
    KSTATUS RegisterWindow(const Window* window);
    Framebuffer* GetFrontBuffer();
    Framebuffer* GetGlobalBuffer();
    private:
    int MouseDeviceHdl = -1;
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
    std::list<Window*> Windows;
    void PutPixel(Framebuffer* fb, int64_t x, int64_t y, uint32_t color);
    void PutRect(Framebuffer* fb, int64_t x, int64_t y, uint64_t w, uint64_t h, uint32_t color);
};