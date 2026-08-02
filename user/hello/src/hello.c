#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <stdint.h>
int main(int argc, const char* argv[], const char* envp[]) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("argc=%d argv[0]=%s envp[0]=%s\r\n", argc, argv[0], envp[0]);
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        printf("term rows=%d cols=%d\r\n", ws.ws_row, ws.ws_col);
    } else {
        printf("ioctl fail.\r\n");
    }

    int fds[2];
    if (pipe(fds) != 0) {
        printf("pipe fail.\r\n");
        return 1;
    }
    printf("made a pipe. read fd (handle): %lu write fd (handle): %lu\r\n", fds[0], fds[1]);
    const char* msg = "hello from pipe";
    write(fds[1], msg, 15);
    char* buf = malloc(32);
    uint64_t n = read(fds[0], buf, sizeof(buf)-1);
    printf("read %ld bytes: '%s'\r\n", n, buf);
    close(fds[0]);
    close(fds[1]);
    free(buf);
    return 0;
}