#pragma once
#include <ah5kos.h>
#include <vector>
#include <string>
#include "dwm.h"
#include <cstdbool>
#include "../../shared/dwmapi.h"

class Window {
    public:
    Window(std::string wname, int w, int h, Dwm* dwm, int x, int y);
    ~Window();
    KSTATUS Draw();
    Framebuffer* GetFb();
    bool FullWindowRedraw = false;
    uint64_t OwningPid;
    WNDHDL Wid;
    private:
    int Width;
    int Height;
    Point Pos;
    std::string Name;
    Framebuffer* Buffer;
    Dwm* dwm;
};