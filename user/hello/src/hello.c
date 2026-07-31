#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

int main(int argc, const char* argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    printf("Hello world from ah5kos!\r\n");
    printf("argc = %d argv[0]=%s\r\n", argc, argv[0]);
    char buf[16];
    int fd = open("initrd:/hi.txt", O_RDONLY);
    if (fd <= -1) {
        perror("open fail.\r\n");
        return 1;
    }

    ssize_t bytes = read(fd, buf, sizeof(buf) - 1);
    if (bytes < 0) {
        perror("read fail.\r\n");
        close(fd);
        return 2;
    }
    buf[bytes] =  '\0';

    write(1, "hi from write syscall! :D", 26);
    printf("\r\n contents of file: \r\n");
    write(1, buf, bytes);
    printf("\r\nok bye.\r\n");
    close(fd);
    return 0;
}