#include "window.h"
#include <ah5kos.h>
#include <assert.h>
#include <cstring>
#include <algorithm>

static volatile WNDHDL WidCount = (WNDHDL)0x1;
Window::Window(std::string wname, int w, int h, Dwm* dwm, int x, int y) {
    this->Width = w;
    this->Height = h;
    this->Name = wname;
    this->Buffer = OsCreateFb((uint32_t)this->Width, this->Height);
    this->dwm = dwm;
    this->Pos.x = x;
    this->Pos.y = y;
    this->OwningPid = 1;
    this->Options = DWMPCK_CRWIN_DEFAULT;
    assert(this->Buffer);
    // printf("dwm: win: window{wname='%s', wndhdl/wid=0x%lx, fbptr=0x%lx, x=%d, y=%d}\r\n", wname.c_str(), WidCount, this->Buffer->ptr, this->Pos.x, this->Pos.y);
    memset((void*)this->Buffer->ptr, 0xFF, this->Buffer->size); // fill white for now
    this->Wid = WidCount;
    WidCount+=1; // ++ throws a compiler err??
    FullWindowRedraw = true;
}

Window::~Window() {
    KSTATUS s = OsFreeFb(this->Buffer);
    assert(s == KSUCCESS);
}

uint64_t Window::DrawDecoration() {
    if ((FullWindowRedraw || DecorationRedraw) && (this->Options == DWMPCK_CRWIN_DEFAULT || this->Options == DWMPCK_CRWIN_WINDOWED)) {
        uint64_t x = CLAMP(this->Pos.x, 0, this->dwm->GetFrontBuffer()->width - this->Width);
        uint64_t y = CLAMP(this->Pos.y, 0, this->dwm->GetFrontBuffer()->height - this->Height);
        this->dwm->PutHollowRect(this->dwm->GetFrontBuffer(), x, y, this->Width, this->Height + DWM_WINDOW_TITLEBAR_HEIGHT, this->IsActive ? DWM_WINDOW_TITLEBAR_COLOUR_ACTIVE : DWM_WINDOW_TITLEBAR_COLOUR_INACTIVE);
        this->dwm->PutHollowRect(this->dwm->GetFrontBuffer(), x + 1, y + 1, this->Width - 2, this->Height - 2, this->IsActive ? DWM_WINDOW_TITLEBAR_COLOUR_ACTIVE : DWM_WINDOW_TITLEBAR_COLOUR_INACTIVE);
        this->dwm->PutHollowRect(this->dwm->GetFrontBuffer(), x + 2, y + 2, this->Width - 4, this->Height - 4, this->IsActive ? DWM_WINDOW_TITLEBAR_COLOUR_ACTIVE : DWM_WINDOW_TITLEBAR_COLOUR_INACTIVE);
        // this->dwm->PutHollowRect(this->dwm->GetFrontBuffer(), this->Pos.x + 4, this->Pos.y + 4, this->Width - 8, this->Height - 8, this->IsActive ? DWM_WINDOW_TITLEBAR_COLOUR_ACTIVE : DWM_WINDOW_TITLEBAR_COLOUR_INACTIVE);
        this->dwm->PutRect(this->dwm->GetFrontBuffer(), x, y, this->Width, DWM_WINDOW_TITLEBAR_HEIGHT, this->IsActive ? DWM_WINDOW_TITLEBAR_COLOUR_ACTIVE : DWM_WINDOW_TITLEBAR_COLOUR_INACTIVE);
        OsDrawTextAtFb(this->dwm->GetFrontBuffer(), this->Name.c_str(), x, y, DWM_WINDOW_TITLEBAR_TEXTCOLOUR, this->IsActive ? DWM_WINDOW_TITLEBAR_COLOUR_ACTIVE : DWM_WINDOW_TITLEBAR_COLOUR_INACTIVE);
        
        if (DecorationRedraw) DecorationRedraw = false;
        return DWM_WINDOW_TITLEBAR_HEIGHT;
    }
    return 0;
}

KSTATUS Window::Draw() {
    KSTATUS s = KSUCCESS;
    uint64_t off = DrawDecoration();
    if (FullWindowRedraw)  { 
        uint64_t x = CLAMP(this->Pos.x, 0, this->dwm->GetFrontBuffer()->width - this->Width);
        uint64_t y = CLAMP(this->Pos.y + off, 0, this->dwm->GetFrontBuffer()->height - this->Height);
        s = OsDrawFb(this->dwm->GetFrontBuffer(), this->Buffer, x, y);
    }
    FullWindowRedraw = false;
    return s;
}

KSTATUS Window::DrawDirty() {
    KSTATUS s = KSUCCESS;
    if (FullWindowRedraw) {
        uint64_t off = DrawDecoration();
        uint64_t x = CLAMP(this->Pos.x, 0, this->dwm->GetFrontBuffer()->width - this->Width);
        uint64_t y = CLAMP(this->Pos.y + off, 0, this->dwm->GetFrontBuffer()->height - this->Height);
        s = OsDrawFb(this->dwm->GetFrontBuffer(), this->Buffer, x, y);
        s = OsDrawFbPart(this->dwm->GetGlobalBuffer(), this->dwm->GetFrontBuffer(), x, y, this->Width, this->Height);
        this->FullWindowRedraw = false;
    }
    return s;
}

void Window::ChangeName(std::string newname) {
    this->Name = newname;
}

Framebuffer* Window::GetFb() {return this->Buffer;}
Point& Window::GetPos() {return this->Pos;}
int Window::GetHeight() {return this->Height;}
int Window::GetWidth() {return this->Width;}