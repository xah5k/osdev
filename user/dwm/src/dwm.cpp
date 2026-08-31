#include <stdio.h>
#include <ah5kos.h>
#include "dwm.h"
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include "window.h"

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

KSTATUS Dwm::HandleMouse(int handle) {
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
    if (mouse->LeftClickPress) {
        PutRect(this->CursorBuffer, 0, 0, 5, 5, ARGB(255, 0, 255, 0));
    } else if (mouse->RightClickPress) {
        PutRect(this->CursorBuffer, 0, 0, 5, 5, ARGB(255, 0, 0, 255));
    } else if (mouse->MiddleClickPress) {
        PutRect(this->CursorBuffer, 0, 0, 5, 5, ARGB(255, 255, 0, 0));
    } else {
        PutRect(this->CursorBuffer, 0, 0, 5, 5, ARGB(255, 255, 255, 255));
    }
    return KSUCCESS;
}

KSTATUS Dwm::Draw() {
    for (Window* window : this->Windows) {
        window->Draw();
    }
    if (this->DrawFullBuffer) {
        KSTATUS s = OsDrawFb(this->GlobalBuffer, this->FrontBuffer, 0, 0);
        this->DrawFullBuffer = false;
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
    while (1) {
        this->HandleMouse(MouseDeviceHdl);
        this->Draw();
    }
}

Dwm::~Dwm() {
    free(this->GlobalBuffer);
    KSTATUS s = OsFreeFb(this->CursorBuffer);
    s = OsFreeFb(this->FrontBuffer);
    this->Status = s; // idrc
}
KSTATUS Dwm::RegisterWindow(const Window* window) {
    this->Windows.push_back((Window*)window);
    return KSUCCESS;
}

KSTATUS Dwm::GetStatus() {
    return this->Status;
}

Framebuffer* Dwm::GetFrontBuffer() {
    return this->FrontBuffer;
}

Framebuffer* Dwm::GetGlobalBuffer() {
    return this->GlobalBuffer;
}