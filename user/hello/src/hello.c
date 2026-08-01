#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

int main(int argc, const char* argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);
    DIR* f = opendir("initrd:/programs");
    if (!f) {
        printf("fail to open directory.\r\n");
        return -1;
    }
    printf("acquire dir 0x%lx\r\n", f);
    struct dirent* dent = readdir(f);
    while (dent->d_type != 0) {
        printf("dir: %d : %s\r\n", dent->d_type, dent->d_name);
        dent = readdir(f);
    }
    return 0;
}