#include "dwm.h"

// hack cuz i didnt compile stuff properly
// void *__gxx_personality_v0;

extern "C" int main(int argc, const char* argv[]) {
    Dwm dwm = Dwm("krnlfs:/Devices/ps2mouse");
    KSTATUS s = dwm.GetStatus();
    return s;
}