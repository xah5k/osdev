#include <stdio.h>
#include <ah5kos.h>
#include "../../shared/dwmapi.h"
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>


int main(int argc, const char* argv[]) {
    DwmApiInitalize();
    WNDHDL Window = NULL;
    Framebuffer* Fb = malloc(sizeof(Framebuffer));
    DwmApiCreateWin(256, 256, 256, 256, DWMPCK_CRWIN_WINDOWED, &Window, Fb);
    if (!Window) {
        perror("window creation fail");
        return -1;
    }
    DwmApiChangeName(Window, "Test program");
    printf("created window.\r\n");
    PutRect(Fb, 64, 64, 10, 10, ARGB(255, 255, 0, 0));
    OsPutcAtFb(Fb, 'A', 128, 128, ARGB(255, 255, 0, 0), ARGB(255, 255, 255, 255));
    OsDrawTextAtFb(Fb, "Hello world from program!", 0, 0, ARGB(255, 0, 0, 255), ARGB(255, 255, 255, 255));
    DwmApiDrawFinish(Window);
    while (1);
    return 0;
}