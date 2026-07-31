#include "shell.h"
#include <external/printf.h>
#include <fb.h>
#include <util/kbdtransl.h>
#include <mm/heap.h>
#include <util/util.h>
#include <fs/vfs.h>
#include <ksyscall.h>
#include <memory.h>
#include <sched/process.h>
static void KeShlTestLs(const char* path) {
    printf("kernel: ls: Listing for %s\r\n", path);
    VfsDirEntry dirent;
    int idx = 0;
    int handle = OsOpen(path, 0);
    if (handle < 0) { printf("kernel: ls: get handle for path fail.\r\n"); return; }
    int result = OsReadDir(handle, &dirent, idx);
    if (result < 0) { printf("kernel: ls: readdir fail.\r\n"); return; }
    printf("kernel: ls: return code of 1st attempt = %d\r\n", result);
    while (result == 1) {
        result = OsReadDir(handle, &dirent, idx);
        const char* Type = (dirent.Type == VFS_TYPE_DIRECTORY) ? "<DIR>" : "     ";
        printf("    %s  %s\r\n", Type, dirent.Name);
        idx++;
    }
    OsClose(handle);
    printf("kernel: ls: total entries %d\r\n", idx);
}

char* KeShlReadStr() {
    char* strbuf = MmAllocate(sizeof(char) * 1024);
    int index = 0;
    memset((void*)strbuf, 0, 1024);
    while (1) {
        char c = KbdTranslGetc();
        if (c != 0) {
            if (c == '\n') {
                strbuf[index] = 0;
                break;
            }
            if (c == '\b') {
                strbuf[index] = 0;
                index--;
                _putchar('\b');
                _putchar(' ');
                _putchar('\b');
                continue;
            }
            strbuf[index] = c;
            _putchar(c);
            index++;
        }
    }
    strbuf[index] = 0;
    return strbuf;
}

void KeShlProcess(char* string) {
    printf("\r\n");
    if (strcmp(string, "ver") == 0) {
        printf("version: 1.0.0\r\n");
    } else if (strcmp(string, "echo") == 0) {
        printf("enter arg: ");
        char* s = KeShlReadStr();
        printf("\r\n%s", s);
        MmFree(s);
    } else if (strcmp(string, "help") == 0) {
        printf("available commands: \r\n");
        printf("ver - display kernel version.\r\n");
        printf("echo - echo something to the screen!\r\n");
        printf("ls - list directory\r\n");
        printf("exec - execute a user mode binary.\r\n");
        printf("lsproc - list processes and threads.\r\n");
    } else if (strcmp(string, "ls") == 0) {
        printf("enter path: ");
        char* s = KeShlReadStr();
        printf("\r\n");
        KeShlTestLs((const char*)s);
        MmFree(s);
    } else if (strcmp(string, "exec") == 0) {
        printf("enter path: ");
        char* s = KeShlReadStr();
        printf("\r\n");
        printf("enter num of args: ");
        char* argcstr = KeShlReadStr();
        int argc = AsciiAsInt(argcstr);
        MmFree(argcstr);
        char** argv = MmAllocate(argc+1 * sizeof(char*));
        for (int i = 0; i < argc; i++) {
            printf("\r\nenter argv[%d]: ", i);
            char* str = KeShlReadStr();
            argv[i] = MmAllocate(strlen(str)+1 * sizeof(char));
            memcpy((void*)argv[i], (const void*)str, strlen(str)+1);
        }
        argv[argc] = NULL;
        uint64_t pid = KE_SYSCALL_CALL_ARG3(SysSpawn, (uint64_t)s, (uint64_t)argv, (uint64_t)argc);
        printf("\r\nspawned process with pid %d\r\n", pid);
        uint64_t r = KE_SYSCALL_CALL_ARG1(SysWaitPid, pid);
        printf("\r\nprocess exited with code %d\r\n", r);
        MmFree(s);
    } else if (strcmp(string, "lsproc") == 0) {
        ProcListRunning(KernelGetInformation());
        printf("\r\n");
    }
    else {
        if (strcmp(string, "") != 0) printf("error: no such command '%s' \r\n", string);
    }
}
void KeUtilShell() {
    FbClear();
    printf("kshell: Welcome to ah5kos 1.0.0\r\n");
    printf("kshell> ");
    while (1) {
        char* str = KeShlReadStr();
        KeShlProcess(str);
        MmFree(str);
        printf("\r\nkshell> ");
    }
}