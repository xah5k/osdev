#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

void print_stat(const char* label, struct stat* st) {
    printf("%s: mode=0%o size=%ld isdir=%d isreg=%d\r\n", label, (unsigned int)st->st_mode, (long)st->st_size, S_ISDIR(st->st_mode), S_ISREG(st->st_mode));
}

int main(int argc, const char* argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);
    struct stat st;

    if (stat("initrd:/programs", &st) == 0) {
        print_stat("stat(programs dir)", &st);
    } else {
        printf("stat(programs dir) FAILED\r\n");
    }

    if (stat("initrd:/programs/hello.elf", &st) == 0) {
        print_stat("stat(hello.elf)", &st);
    } else {
        printf("stat(hello.elf) FAILED\r\n");
    }

    if (stat("initrd:/nope/does/not/exist", &st) == 0) {
        printf("stat(bogus) SUCCEEDED (unexpected!)\r\n");
    } else {
        printf("stat(bogus) correctly failed\r\n");
    }

    int fd = open("initrd:/programs/hello.elf", O_RDONLY);
    if (fd >= 0) {
        if (fstat(fd, &st) == 0) {
            print_stat("fstat(hello.elf fd)", &st);
        } else {
            printf("fstat(hello.elf fd) FAILED\r\n");
        }
        close(fd);
    } else {
        printf("open(hello.elf) failed, can't test fstat\r\n");
    }

    if (lstat("initrd:/programs/hello.elf", &st) == 0) {
        print_stat("lstat(hello.elf)", &st);
    } else {
        printf("lstat(hello.elf) FAILED\r\n");
    }

    chdir("initrd:/programs");
    if (stat("hello.elf", &st) == 0) {
        print_stat("stat(relative hello.elf)", &st);
    } else {
        printf("stat(relative hello.elf) FAILED\r\n");
    }

    return 0;
}