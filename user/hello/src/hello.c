
#include <stdio.h>
#include <stdint.h>
#include <ah5kos.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

int main(int argc, const char* argv[]) {
    printf("sleepiong for 5 secs...");
    OsSleep(5);
    printf("sleep done.\r\n");
    return 0;
}