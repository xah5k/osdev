#include <stdio.h>
#include <ah5kos.h>
#include "dwm.h"
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include "window.h"
#include "server.h"
#include <fcntl.h>

#define ARGB(a, r, g, b) (a << 24) | (r << 16) | (g << 8) | b
#define FBOFF(x, y, fbinfo) ((y) * (fbinfo)->scanline + (x) * ((fbinfo)->bpp / 8))

void Dwm::PutPixel(Framebuffer* fb, int64_t x, int64_t y, uint32_t color) {
    uint32_t offset = FBOFF(x, y, fb);
    *(uint32_t*)((uint8_t*)fb->ptr + offset) = color;
}

void Dwm::PutRect(Framebuffer* fb, int64_t x, int64_t y, uint64_t w, uint64_t h, uint32_t color) {
    if (x < 0 || y < 0) return;
    if ((uint64_t)x + w > fb->width) return;
    if ((uint64_t)y + h > fb->height) return;
    uint32_t offset = FBOFF(x, y, fb);
    uint8_t* RowPtr = (uint8_t*)fb->ptr + offset;
    for (uint64_t j = 0; j < h; j++) {
        uint32_t* px = (uint32_t*)RowPtr;
        for (uint64_t i = 0; i < w; i++) {
            px[i] = color;
        }
        RowPtr += fb->scanline;
    }
}

void Dwm::HorizontalLine(Framebuffer* fb, int64_t x, int64_t y, uint32_t length, uint32_t color) {
    PutRect(fb, x, y, length, DWM_WINDOW_BORDER_SIZE, color);
}
void Dwm::VerticalLine(Framebuffer* fb, int64_t x, int64_t y, uint32_t length, uint32_t color) {
    PutRect(fb, x, y, DWM_WINDOW_BORDER_SIZE, length, color);
}

void Dwm::PutHollowRect(Framebuffer* fb, int64_t x, int64_t y, uint64_t w, uint64_t h, uint32_t color) {
    HorizontalLine(fb, x, y, w, color);
    VerticalLine(fb, x, y + 1, h - 2, color);
    HorizontalLine(fb, x, y + h - 1, w, color);
    VerticalLine(fb, x + w - 1, y + 1, h - 2, color);
}

KSTATUS Dwm::HandleInput(int handle, int kbdhandle) {
    KeDevMousePacket* mouse = (KeDevMousePacket*)malloc(sizeof(KeDevMousePacket));
    int s = read(handle, mouse, sizeof(KeDevMousePacket));
    if (s != sizeof(KeDevMousePacket)) {
        free(mouse);
        return KFAIL;
    }
    this->CursorPosX += mouse->RawPos.x;
    this->CursorPosY += mouse->RawPos.y;
    if (this->CursorPosX >= this->GlobalBuffer->width) {
        this->CursorPosX = this->GlobalBuffer->width - 1;
    }
    if (this->CursorPosY >= this->GlobalBuffer->height) {
        this->CursorPosY = this->GlobalBuffer->height - 1;
    }
    if (this->CursorPosX < 0) this->CursorPosX = 0;
    if (this->CursorPosY < 0) this->CursorPosY = 0;
    // printf("mouse RawPos X = %d mouse RawPos Y = %d CurPosX = %d CurPosY = %d\r\n", mouse->RawPos.x, mouse->RawPos.y, CursorPosX, CursorPosY);
    // printf("this->FrontBuffer=0x%lx this->FrontBuffer->ptr=0x%lx this->CursorPosX=0x%lx this->CursorPosY=0x%lx\r\n", this->FrontBuffer, this->FrontBuffer->ptr, this->CursorPosX, this->CursorPosY);
    // printf("fb %dx%d cmp to gfb %dx%d\r\n", this->FrontBuffer->width, this->FrontBuffer->height, this->GlobalBuffer->width, this->GlobalBuffer->height);
    for (Window* wnd : this->Windows) {
        uint32_t x = wnd->GetPos().x;
        uint32_t y = wnd->GetPos().y;
        uint32_t w = wnd->GetWidth();
        uint32_t h = wnd->GetHeight();
        // check bounds
        if (this->CursorPosX >= x && this->CursorPosX < (x + w) && this->CursorPosY >= y && this->CursorPosY < (y + h)) {
            if (mouse->LeftClickPress) {
                if (!this->OldLeftClickPress) {
                    if (FocusedWindow)  { FocusedWindow->IsActive = false; FocusedWindow->FullWindowRedraw = true;}
                    this->Windows.remove(wnd);
                    this->Windows.push_back(wnd);
                    FocusedWindow = wnd;
                    wnd->IsActive = true;
                    wnd->FullWindowRedraw = true;
                    this->Redraw(); // window order has changed
                    // printf("dwm: clicked on window with whdl 0x%lx win x=%d win y=%d win w=%d win h=%d mousepos={%d, %d}\r\n", wnd->Wid, x, y, w, h, this->CursorPosX, this->CursorPosY);
                }
            }
            if (wnd->IsActive) { // only if focused
                uint32_t lx = this->CursorPosX - x;
                uint32_t ly = this->CursorPosY - y;
                lx = lx - ((wnd->Options == DWMPCK_CRWIN_WINDOWED) ? DWM_WINDOW_BORDER_SIZE : 0);
                ly = ly - ((wnd->Options == DWMPCK_CRWIN_WINDOWED) ? DWM_WINDOW_TITLEBAR_HEIGHT : 0);
                if (lx >= 0 && ly >= 0) {
                    DwmReEventResponse r;
                    r.Type = DWMPCK_EVENT_MOUSE;
                    r.Window = wnd->Wid;
                    r.MouseX = lx;
                    r.MouseY = ly; 
                    r.MouseBtnLeft = mouse->LeftClickPress;
                    r.MouseBtnMiddle = mouse->MiddleClickPress;
                    r.MouseBtnRight = mouse->RightClickPress;
                    r.KbdChar = 0;
                    KSTATUS s = this->MsgServer->SendEventMsg(wnd->OwningPid, &r);
                }
            }
            // printf("dwm: send event to pid kstatus 0x%lx\r\n", s);
            break;
        }
    }
    if (mouse->LeftClickPress) {
        PutRect(this->CursorBuffer, 0, 0, 5, 5, ARGB(255, 0, 255, 0));
    } else if (mouse->RightClickPress) {
        PutRect(this->CursorBuffer, 0, 0, 5, 5, ARGB(255, 0, 0, 255));
    } else if (mouse->MiddleClickPress) {
        PutRect(this->CursorBuffer, 0, 0, 5, 5, ARGB(255, 255, 0, 0));
    } else {
        PutRect(this->CursorBuffer, 0, 0, 5, 5, ARGB(255, 255, 255, 255));
    }
    this->OldLeftClickPress = mouse->LeftClickPress;
    return KSUCCESS;
}

void Dwm::RedrawAllWindows() {
    for (Window* window : this->Windows) {
        window->FullWindowRedraw = true;
        window->Draw();
    }
}
KSTATUS Dwm::Draw() {
    if (this->DrawFullBuffer) {
        RedrawAllWindows();
        KSTATUS s = OsDrawFb(this->GlobalBuffer, this->FrontBuffer, 0, 0);
        this->DrawFullBuffer = false;
    } else {
        for (Window* window : this->Windows) {
            if (window->FullWindowRedraw) {
                window->DrawDirty(); // note: draw dirty directly draws onto global buffer
            }
        }
    }
    if (this->CursorPosX == this->OldCursorPosX && this->CursorPosY == this->OldCursorPosY) {
        KSTATUS s = KSUCCESS;
        return s;
    }
    KSTATUS s = OsDrawFbPart(this->GlobalBuffer, this->FrontBuffer, OldCursorPosX, OldCursorPosY, 5, 5);
    s = OsDrawFb(this->GlobalBuffer, this->CursorBuffer, this->CursorPosX, this->CursorPosY);
    this->OldCursorPosX = this->CursorPosX;
    this->OldCursorPosY = this->CursorPosY;
    return s;
}

Dwm::Dwm() {}

void Dwm::Redraw() {
    this->DrawFullBuffer = true;
    // this->Draw();
}

Dwm::Dwm(const char* MousePath) {
    this->GlobalBuffer = (Framebuffer*)malloc(sizeof(Framebuffer));
    KSTATUS s = OsGetFbInfo(this->GlobalBuffer);
    if (s != KSUCCESS) {
        fprintf(stderr, "dwm: failed to get screen information. KSTATUS 0x%lx\r\n", s);
        this->Status = s;
        return;
    }
    printf("dwm: screen %dx%d\r\n", this->GlobalBuffer->width, this->GlobalBuffer->height);
    this->FrontBuffer = OsCreateFb(this->GlobalBuffer->width, this->GlobalBuffer->height);
    this->CursorBuffer = OsCreateFb(5, 5); // change once we use proper sprites
    // printf("dwm: this->FrontBuffer=0x%lx this->FrontBuffer->ptr = 0x%lx with a pxsz of %dx%d\r\n", this->FrontBuffer, this->FrontBuffer->ptr, this->GlobalBuffer->width, this->GlobalBuffer->height);
    if (!this->FrontBuffer || !this->CursorBuffer) {
        fprintf(stderr, "dwm: failed to allocate front or cursor buffer.\r\n");
        this->Status = KOOMERR;
        return;
    }
    int h = open(MousePath, 0);
    if (h <= 0) {
        fprintf(stderr, "dwm: failed to open mouse device.\r\n");
        this->Status = KINVALID;
        return;
    }
    this->MouseDeviceHdl = h;
}

void Dwm::Start() {
    if (this->MouseDeviceHdl == -1) {
        fprintf(stderr, "dwm: cannot start! invalid mouse handle.\r\n");
        this->Status = KINVALID;
        return;
    }
    this->DrawFullBuffer = true;
    this->MsgServer = new Server(this);
    while (1) {
        this->HandleInput(MouseDeviceHdl, STDIN_FILENO);
        this->Draw();
    }
}

Dwm::~Dwm() {
    free(this->GlobalBuffer);
    KSTATUS s = OsFreeFb(this->CursorBuffer);
    s = OsFreeFb(this->FrontBuffer);
    this->Status = s; // idrc
    delete this->MsgServer;
    close(this->MouseDeviceHdl);
}

KSTATUS Dwm::RegisterWindow(const Window* window) {
    this->Windows.push_back((Window*)window);
    return KSUCCESS;
}

KSTATUS Dwm::GetStatus() {
    return this->Status;
}

void Dwm::SetStatus(KSTATUS s) {
    this->Status = s;
}

Framebuffer* Dwm::GetFrontBuffer() {
    return this->FrontBuffer;
}

Framebuffer* Dwm::GetGlobalBuffer() {
    return this->GlobalBuffer;
}

std::list<Window*>& Dwm::GetWindowsList() {
    return this->Windows;
}

KSTATUS Dwm::DeregisterWindow(WNDHDL w) {
    for (Window* w : this->Windows) {
        if (w->Wid == w) {
            this->Windows.remove(w);
            this->DrawFullBuffer = true;
            delete w; // for now, whoever deregisters it should delete it on their own
            return KSUCCESS;
        }
    }
    return KFAIL;
}