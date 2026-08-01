#include <stdio.h>
#include <sys/utsname.h>
#include <stdlib.h>

int main(int argc, const char* argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("uname: hello from userspace!\r\n");
    struct utsname* buf = malloc(sizeof(struct utsname));
    if (uname(buf) != 0) {
        printf("uname: syscall fail.\r\n");
        return -1;
    }
    printf("%s %s %s %s %s\r\n", buf->sysname, buf->nodename, buf->version, buf->release, buf->domainname);
    return 0;
}