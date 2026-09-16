#pragma once
#include <ah5kos.h>
#include "dwm.h"
#include <string>
#include "../../shared/dwmapi.h"


class Server {
    public:
    Server(Dwm* dwm);
    ~Server();
    OsMessage* CreateFbInfoMsg(Framebuffer* info, WNDHDL whdl);
    OsMessage* CreateFbPtrMsg(Framebuffer* msg);
    Window* CreateWindow(std::string wname, int w, int h, int x, int y);
    Dwm* dwm;
    private:
};