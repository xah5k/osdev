#pragma once
#include <ah5kos.h>
#include <vector>
#include <string>
#include "dwm.h"
#include <cstdbool>

class Window {
    public:
    Window(std::string wname, int w, int h, Dwm* dwm, int x, int y);
    ~Window();
    KSTATUS Draw();
    bool FullWindowRedraw = false;
    private:
    int Width;
    int Height;
    Point Pos;
    std::string Name;
    Framebuffer* Buffer;
    Dwm* dwm;
};