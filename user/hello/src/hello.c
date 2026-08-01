#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, const char* argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);
    int fd = open("initrd:/hi.txt", 0); // mode currently doesnt even matter
    if (fd < 0) {
        printf("open failed\r\n");
        return 1;
    }
    char buf[32];

    ssize_t n = read(fd, buf, 4);
    buf[n] = '\0';
    printf("first 4 bytes: '%s'\r\n", buf);

    off_t pos = lseek(fd, 0, SEEK_SET);
    printf("seek to 0, lseek returned: %ld\r\n", (long)pos);

    n = read(fd, buf, 4);
    buf[n] = '\0';
    printf("after seek(0), read again: '%s'\r\n", buf);

    pos = lseek(fd, 2, SEEK_CUR);
    printf("seek +2 from current, lseek returned: %ld\r\n", (long)pos);

    n = read(fd, buf, 4);
    buf[n] = '\0';
    printf("after seek(+2, CUR): '%s'\r\n", buf);

    pos = lseek(fd, 0, SEEK_END);
    printf("seek to end, lseek returned (file size): %ld\r\n", (long)pos);

    n = read(fd, buf, 4);
    printf("read at EOF returned: %ld bytes\r\n", (long)n);

    pos = lseek(fd, -3, SEEK_END);
    printf("seek -3 from end, lseek returned: %ld\r\n", (long)pos);
    n = read(fd, buf, 3);
    buf[n] = '\0';
    printf("last 3 bytes: '%s'\r\n", buf);

    close(fd);
    return 0;
}