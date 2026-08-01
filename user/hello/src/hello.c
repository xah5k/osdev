#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

int main(int argc, const char* argv[], const char* envp[]) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("argc=%d argv[0]=%s envp[0]=%s\r\n", argc, argv[0], envp[0]);
    return 0;
}