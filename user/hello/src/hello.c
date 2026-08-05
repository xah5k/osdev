#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
int main() {
    int fds[2];
    pipe(fds);

    int pid = fork();
    if (pid == 0) {
        // yes yes my child
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        close(fds[1]);

        const char* args[] = { "uname.elf", NULL };
        const char* envp[] = {"dontsetenvptonullotherwisebadthingswillhappen", NULL };
        execve("initrd:/programs/uname.elf", (char**)args, (char**)envp);

        printf("execve failed!\r\n");
        return 1;
    } else {
        // read whatever child starts crying about
        close(fds[1]);
        char buf[256] = {0};
        int total = 0;
        int n;
        waitpid(pid, NULL, 0);
        while ((n = read(fds[0], buf + total, sizeof(buf) - total - 1)) > 0) {
            total += n;
        }
        printf("child whined %d bytes:\r\n%s\r\n", total, buf);
        close(fds[0]);
    }

    return 0;
}