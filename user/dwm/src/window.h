#pragma once
#include <ah5kos.h>
#include <vector>
#include <string>
#include "dwm.h"
#include <cstdbool>
#include "../../shared/dwmapi.h"

#define DWM_WINDOW_TITLEBAR_COLOUR_ACTIVE ARGB(255, 58, 97, 243)
#define DWM_WINDOW_TITLEBAR_COLOUR_INACTIVE ARGB(128, 143, 166, 250)
#define DWM_WINDOW_TITLEBAR_TEXTCOLOUR ARGB(255, 255, 255, 255)
#define DWM_WINDOW_TITLEBAR_HEIGHT 31
#define DWM_WINDOW_BORDER_SIZE 3

// useful for windows thats width and height might go off the screen
#define CLAMP(v, lo, hi) \
    std::max<std::common_type_t<decltype(lo), decltype(hi)>> \
        (lo, std::min<std::common_type_t<decltype(v), decltype(hi)>>(v, hi)) // holy shit c++ is ugly only reason why i used it was for oop classes

class Window {
    public:
    Window(std::string wname, int w, int h, Dwm* dwm, int x, int y);
    ~Window();
    KSTATUS Draw();
    KSTATUS DrawDirty();
    uint64_t DrawDecoration();
    Framebuffer* GetFb();
    bool FullWindowRedraw = false;
    bool IsActive = false;
    bool DecorationRedraw = false;
    uint64_t OwningPid;
    WNDHDL Wid;
    DwmCrWinOpt Options;
    Point& GetPos();
    int GetWidth();
    int GetHeight();
    void ChangeName(std::string newname);
    private:
    int Width;
    int Height;
    Point Pos;
    std::string Name;
    Framebuffer* Buffer;
    Framebuffer* DecorationBuffer;
    Dwm* dwm;
};