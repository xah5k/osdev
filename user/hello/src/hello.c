#include <stdio.h>
#include <unistd.h>

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("before fork\r\n");

    int pid = fork();
    printf("fork pid = %d\r\n", pid);
    if (pid == 0) {
        // child: execve directly, no redirection
        printf("child: about to execve\r\n");

        const char* args[] = { "uname.elf", NULL };
        const char* envp[] = { "TEST=value", NULL };
        execve("initrd:/programs/uname.elf", (char**)args, (char**)envp);

        // only reached if execve failed
        printf("execve failed!\r\n");
        return 1;
    } else {
        // parent: just wait a bit and print
        printf("parent: forked child with pid %d\r\n", pid);
        for (volatile int i = 0; i < 50000000; i++);
        printf("parent: done waiting\r\n");
    }

    return 0;
}