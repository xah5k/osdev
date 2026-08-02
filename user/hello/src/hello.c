#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/ioctl.h>
int main(int argc, const char* argv[], const char* envp[]) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("argc=%d argv[0]=%s envp[0]=%s\r\n", argc, argv[0], envp[0]);
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        printf("term rows=%d cols=%d\r\n", ws.ws_row, ws.ws_col);
    } else {
        printf("ioctl fail.\r\n");
    }
    return 0;
}