#include "dwm.h"
#include "window.h"
// hack cuz i didnt compile stuff properly
// void *__gxx_personality_v0;

Window* MainCreateWindow(std::string wname, int w, int h, Dwm* dwm, int x, int y) {
    Window* window = new Window(wname, w, h, dwm, x, y);
    dwm->RegisterWindow((const Window*)window);
    return window;
}

extern "C" int main(int argc, const char* argv[]) {
    Dwm dwm = Dwm("krnlfs:/Devices/ps2mouse");
    // create a bunch of windows before starting
    // MainCreateWindow("window1", 50, 50, &dwm, 128, 128);
    // MainCreateWindow("window2", 100, 100, &dwm, 256, 256);
    MainCreateWindow("window3", 200, 200, &dwm, 512, 512);
    dwm.Start();
    KSTATUS s = dwm.GetStatus();
    return s;
}