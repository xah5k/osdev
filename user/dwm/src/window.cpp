#include "window.h"
#include <ah5kos.h>
#include <assert.h>
#include <cstring>
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
    assert(this->Buffer);
    printf("dwm: win: window{wname='%s', wndhdl/wid=0x%lx, fbptr=0x%lx}\r\n", wname.c_str(), WidCount, this->Buffer->ptr);
    memset((void*)this->Buffer->ptr, 0xFF, this->Buffer->size); // fill white for now
    this->Wid = WidCount;
    WidCount+=1; // ++ throws a compiler err??
    FullWindowRedraw = true;
}

Window::~Window() {
    KSTATUS s = OsFreeFb(this->Buffer);
    assert(s == KSUCCESS);
}

KSTATUS Window::Draw() {
    KSTATUS s = KSUCCESS;
    if (FullWindowRedraw) s = OsDrawFb(this->dwm->GetFrontBuffer(), this->Buffer, this->Pos.x, this->Pos.y);
    FullWindowRedraw = false;
    return s;
}

Framebuffer* Window::GetFb() {return this->Buffer;}