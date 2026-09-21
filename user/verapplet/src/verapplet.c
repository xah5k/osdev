#include <stdio.h>
#include <ah5kos.h>
#include "../../shared/dwmapi.h"
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>
#include <sys/utsname.h>

int main(int argc, const char* argv[]) {
    DwmApiInitalize();
    WNDHDL Window = NULL;
    Framebuffer* Fb = malloc(sizeof(Framebuffer));
    DwmApiCreateWin(256, 256, DWMPCK_CRWIN_WINDOWED, &Window, Fb);
    if (!Window) {
        perror("window creation fail");
        return -1;
    }
    printf("%d: created window.\r\n", getpid());
    struct utsname* buf = malloc(sizeof(struct utsname));
    if (uname(buf) != 0) {
        perror("syscall fail.");
        return -1;
    }
    char* buffer = malloc(128);
    memset(buffer, 0, 128);
    snprintf(buffer, 128, "Ah5kOs Version %s", buf->release);
    OsDrawTextAtFb(Fb, buffer, 0, 64, ARGB(255, 0, 0, 255), ARGB(255, 255, 255, 255));
    free(buffer);
    printf("%d: finished drawing. notifying dwm..\r\n", getpid());
    DwmApiDrawFinish(Window);
    while (1);
    return 0;
}