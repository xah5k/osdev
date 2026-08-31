#include "window.h"
#include <ah5kos.h>
#include <assert.h>
#include <cstring>
Window::Window(std::string wname, int w, int h, Dwm* dwm, int x, int y) {
    this->Width = w;
    this->Height = h;
    this->Name = wname;
    this->Buffer = OsCreateFb((uint32_t)this->Width, this->Height);
    this->dwm = dwm;
    this->Pos.x = x;
    this->Pos.y = y;
    assert(this->Buffer);
    memset((void*)this->Buffer->ptr, 0xFF, this->Buffer->size); // fill white for now
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