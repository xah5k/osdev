#pragma once
#include <ah5kos.h>
#include <stdbool.h>
#include <list>
#include "../../shared/dwmapi.h"
class Window;
class Server;

class Dwm {
    public:
    Dwm();
    Dwm(const char* MousePath);
    ~Dwm();
    void Start();
    KSTATUS GetStatus();
    void SetStatus(KSTATUS s);
    KSTATUS RegisterWindow(const Window* window);
    KSTATUS DeregisterWindow(WNDHDL w);
    Framebuffer* GetFrontBuffer();
    Framebuffer* GetGlobalBuffer();
    void Redraw();
    void RedrawAllWindows();
    std::list<Window*>& GetWindowsList();
    void PutPixel(Framebuffer* fb, int64_t x, int64_t y, uint32_t color);
    void PutRect(Framebuffer* fb, int64_t x, int64_t y, uint64_t w, uint64_t h, uint32_t color);
    private:
    std::list<Window*> Windows;
    int MouseDeviceHdl = -1;
    KSTATUS HandleMouse(int handle);
    KSTATUS Draw();
    int64_t CursorPosX = 0;
    int64_t CursorPosY = 0;
    int64_t OldCursorPosX = 0;
    int64_t OldCursorPosY = 0;
    int OldLeftClickPress = 0;
    Framebuffer* FrontBuffer;
    Framebuffer* CursorBuffer;
    Framebuffer* GlobalBuffer;
    Window* FocusedWindow = NULL;
    KSTATUS Status = KSUCCESS;
    bool DrawFullBuffer = false;
    Server* MsgServer;
};