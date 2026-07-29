#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

int main(int argc, const char* argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    printf("Hello world from ah5kos!\r\n");
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

    unsigned char scancode;
    char hex_log[] = "key press: 0x00 scancode.\r\n";
    const char* hex_table = "0123456789abcdef";

    while (1) {
        if (read(STDIN_FILENO, &scancode, 1) == 1) {
            hex_log[13] = hex_table[(scancode >> 4) & 0x0F];
            hex_log[14] = hex_table[scancode & 0x0F];
            write(STDOUT_FILENO, hex_log, 27);
        }
    }
    return 0;
}