#include <stdio.h>
#include <unistd.h>

int main(int argc, const char* argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);
    char* path = "initrd:/programs/uname.elf";
    char const* _argv[] = {"hello.elf", NULL};
    char const* _envp[] = {"PLACEHOLDER=HI", NULL};
    printf("doing exec.\r\n");
    execve(path, (char* const*)_argv, (char* const*)_envp);
    printf("fail.\r\n");
    return 1;
}