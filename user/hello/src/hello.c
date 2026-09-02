
#include <stdio.h>
#include <stdint.h>
#include <ah5kos.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

int main(int argc, const char* argv[]) {
    int h = open("ext2_2:/test.txt", O_CREAT | O_RDWR);
    printf("return handle %d.\r\n", h);
    const char str[] = "Hello world!\r\n";
    int s = write(h, (void*)str, 15);
    printf("return of write call. %d\r\n", s);
    close(h);
    return 0;
}