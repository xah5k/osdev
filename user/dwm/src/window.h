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