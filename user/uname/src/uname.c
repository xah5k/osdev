#include <stdio.h>
#include <sys/utsname.h>
#include <stdlib.h>
#include <ah5kos.h>
#include <string.h>
#include <unistd.h>

int main(int argc, const char* argv[]) {
    printf("Hello world.!\r\n");
    printf("sending message to pid 1.\r\n");
    OsMessage* m = malloc(sizeof(OsMessage) + 26);
    memset((void*)m, 0, sizeof(OsMessage) + 26);
    m->FromPid = getpid();
    m->ToPid = 1;
    m->Length = 26;
    const char* str = "Hello from other program.";
    memcpy((void*)((uint64_t)m + sizeof(OsMessage)), str, 26);
    KSTATUS s = OsMsgSend(m);
    printf("sent message. KSTATUS 0x%lx\r\n", s);
    free(m);
    return 0;
}