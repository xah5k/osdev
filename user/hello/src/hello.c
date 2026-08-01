#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

int main(int argc, const char* argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);
    int x = access("initrd:/programs", F_OK);
    printf("result = %d after try access 'initrd:/programs'\r\n", x);
    x = access("initrd:/nonexisting.txt", F_OK);
    printf("result = %d after try access 'initrd:/nonexisting.txt'\r\n", x);
    x = access("initrd:/hi.txt", F_OK);
    printf("result = %d after try access 'initrd:/hi.txt'\r\n", x);
    return 0;
}