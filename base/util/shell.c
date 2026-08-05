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
#include <external/posix/stat.h>
#ifdef __x86_64__
#include <arch/x86_64/pci/pci.h>
#endif
#include <disk/ahci.h>
char** gKeEnvp;
int gKeEnvc = 0;

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

void KeShlFillEnv(const char* env) {
    gKeEnvc += 1;
    gKeEnvp[gKeEnvc-1] = MmAllocate(strlen(env)+1);
    memcpy((void*)gKeEnvp[gKeEnvc-1], (const void*)env, strlen(env)+1);
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
        printf("fsisabsol - checks if a path is absolute.\r\n");
        printf("chdir - changes proc cwd.\r\n");
        printf("getcwd - gets current working directory.\r\n");
        printf("stat - posix-compat function that returns a stat struct.\r\n");
        printf("getenv - dumps environment variables.\r\n");
        printf("addenv - adds an env variable.\r\n");
        printf("iopipe - test IoPipeObj and see if vfs is working with it.\r\n");
        printf("heapdump - dumps heap regions.\r\n");
        printf("cpufeats - cpu features.\r\n");
        printf("lspci - list pci devices.\r\n");
        printf("lsahciport - list ahci ports.\r\n");
    } else if (strcmp(string, "ls") == 0) {
        printf("enter path: ");
        char* s = KeShlReadStr();
        printf("\r\n");
        if (strcmp(s, "") != 0) KeShlTestLs((const char*)s);
        else {
            char* cwdbuf = MmAllocate(VFS_MAX_ALLOWED_PATH);
            KE_SYSCALL_CALL_ARG2(SysGetCwd, (uint64_t)cwdbuf, VFS_MAX_ALLOWED_PATH);
            KeShlTestLs((const char*)cwdbuf);
            MmFree(cwdbuf);
        }
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
        uint64_t pid = SysSpawn((uint64_t)s, (uint64_t)argv, (uint64_t)argc, (uint64_t)gKeEnvp, (uint64_t)gKeEnvc);
        printf("\r\nspawned process with pid %d\r\n", pid);
        uint64_t r = KE_SYSCALL_CALL_ARG1(SysWaitPid, pid);
        printf("\r\nprocess exited with code %d\r\n", r);
        MmFree(s);
    } else if (strcmp(string, "lsproc") == 0) {
        ProcListRunning(KernelGetInformation());
        printf("\r\n");
    } else if (strcmp(string, "fsisabsol") == 0) {
        printf("enter path: ");
        char* s = KeShlReadStr();
        printf("\r\n");
        printf("path is %s\r\n", VfsIsAbsolute(s) ? "absolute" : "relative");
        MmFree(s);
    } else if (strcmp(string, "getcwd") == 0) {
        char* cwdbuf = MmAllocate(VFS_MAX_ALLOWED_PATH);
        KE_SYSCALL_CALL_ARG2(SysGetCwd, (uint64_t)cwdbuf, VFS_MAX_ALLOWED_PATH);
        printf("cwd is %s\r\n", cwdbuf);
        MmFree(cwdbuf);
    } else if (strcmp(string, "chdir") == 0) {
        printf("enter path: ");
        char* dir = KeShlReadStr();
        printf("\r\n");
        uint64_t r = KE_SYSCALL_CALL_ARG1(SysChdir, (uint64_t)dir);
        if (r == 0) printf("cwd is now %s\r\n", dir); else printf("SysChdir failed.\r\n");
        MmFree(dir);
    } else if (strcmp(string, "stat") == 0) {
        printf("enter path: ");
        char* path = KeShlReadStr();
        printf("\r\n");
        posixstat* stat = MmAllocate(sizeof(posixstat));
        uint64_t r = KE_SYSCALL_CALL_ARG2(SysStat, (uint64_t)path, (uint64_t)stat);
        if (r == 0) {
            printf("stat->st_dev = %ld\r\n", stat->st_dev);
            printf("stat->st_ino = %ld\r\n", stat->st_ino);
            printf("stat->st_mode = %ld\r\n", stat->st_mode);
            printf("stat->st_nlink = %ld\r\n", stat->st_nlink);
            printf("stat->st_uid = %ld\r\n", stat->st_uid);
            printf("stat->st_gid = %ld\r\n", stat->st_gid);
            printf("stat->st_rdev = %ld\r\n", stat->st_rdev);
            printf("stat->st_size = %ld\r\n", stat->st_size);
            printf("stat->st_blksize = %ld\r\n", stat->st_blksize);
            printf("stat->st_blocks = %ld\r\n", stat->st_blocks);
            printf("stat->atime = %ld\r\n", stat->st_atime);
            printf("stat->mtime = %ld\r\n", stat->st_mtime);
            printf("stat->ctime = %ld\r\n", stat->st_ctime);
        } else {
            printf("SysStat failed.\r\n");
        }
        MmFree(path);
    } else if (strcmp(string, "getenv") == 0) {
        for (int i = 0; i < gKeEnvc; i++) {
            printf("%s\r\n", gKeEnvp[i]);
        }
    } else if (strcmp(string, "addenv") == 0) {
        printf("enter env (KEY=VAL format): ");
        char* env = KeShlReadStr();
        printf("\r\n");
        KeShlFillEnv(env);
        MmFree(env);
    } else if (strcmp(string, "iopipe") == 0) {
        IoPipeObj* pipe = IoCreatePipe(ThrGetCurrent()->ParentProc);
        if (!pipe) {
            printf("failed to create pipe.\r\n");
            return;
        }
        printf("created new pipe obj @ 0x%lx\r\n", pipe);
        printf("read handle = %lu\r\n", pipe->ReadHandle);
        printf("write handle = %d\r\n", pipe->WriteHandle);
        printf("enter message to write to pipe: \r\n");
        char* msg = KeShlReadStr();
        printf("\r\n");
        int written = OsWrite(pipe->WriteHandle, msg, strlen(msg));
        printf("written %d bytes.\r\n", written);
        printf("testing read now..\r\n");
        char* buf = MmAllocate(64);
        int readn = OsRead((int)pipe->ReadHandle, buf, sizeof(buf) - 1);
        printf("read %d bytes: '%s'\r\n", readn, buf);
        int readn2 = OsRead((int)pipe->ReadHandle, buf, sizeof(buf) - 1);
        printf("read from empty pipe returned %d\r\n", readn2);
        for (int i = 0; i < 20; i++) {
            char smallmsg[8];
            snprintf(smallmsg, sizeof(smallmsg), "m%d", i);
            int w = OsWrite((int)pipe->WriteHandle, smallmsg, strlen(smallmsg));
            char rbuf[8] = {0};
            int r = OsRead((int)pipe->ReadHandle, rbuf, sizeof(rbuf) - 1);
            printf("iter %d wrote=%d read='%s'(%d)\r\n", i, w, rbuf, r);
        }
        OsClose((int)pipe->ReadHandle);
        OsClose((int)pipe->WriteHandle);
        MmFree(buf);
        MmFree(msg);
        printf("closed both directions.\r\n");
    } else if (strcmp(string, "heapdump") == 0) {
        MmHeapDumpMap();
    } else if (strcmp(string, "cpufeats") == 0) {
        CpuFeatures* f = KernelGetInformation()->cpufeats;
        printf("cpu features: \r\n");
        printf("f->smap = %d\r\n", f->smap);
    } else if (strcmp(string, "lspci") == 0) {
        KePciDeviceHdr* hdr = PciGetLinkedList();
        if (!hdr) {
            printf("error: no list.\r\n");
            return;
        }
        while (hdr != NULL) {
            printf("found device %lx:%lx\r\n", hdr->Header->VendorID, hdr->Header->DeviceID);
            hdr = hdr->Next;
        }
    } else if (strcmp(string, "lsahciport") == 0) {
        for (int i = 0; i < 32; i++) {
            KeAhciPort* port = AhciGetPort((uint8_t)i);
            if (port) {
                if (port->HbaType == AHCI_TYPE_SATA) printf("port %d: type=SATA HbaPort=0x%lx\r\n", i, port->HbaPort);
                if (port->HbaType == AHCI_TYPE_SATAPI) printf("port %d: type=SATA HbaPort=0x%lx\r\n", i, port->HbaPort);
            }
        }
    }
    else {
        if (strcmp(string, "") != 0) printf("error: no such command '%s' \r\n", string);
    }
}
extern uint64_t PmmTotalPhysicalMem;

void KeUtilShell() {
    FbClear();
    printf("kshell: Welcome to ah5kos 1.0.0\r\n");
    printf("kshell: Total Physical RAM: %d MB\r\n", PmmTotalPhysicalMem /  1048576);
    char* cwdbuf = MmAllocate(VFS_MAX_ALLOWED_PATH);
    KE_SYSCALL_CALL_ARG2(SysGetCwd, (uint64_t)cwdbuf, VFS_MAX_ALLOWED_PATH);
    printf("kshell: cwd is %s\r\n", cwdbuf);
    gKeEnvp = MmAllocate(sizeof(char*)*64);
    KeShlFillEnv("HOME=initrd:/home");
    KeShlFillEnv("TERM=ah5kos");
    KeShlFillEnv("PATH=initrd:/programs");
    KeShlFillEnv("PRIV=kernel");
    MmFree(cwdbuf);
    printf("kshell> ");
    while (1) {
        char* str = KeShlReadStr();
        KeShlProcess(str);
        MmFree(str);
        printf("\r\nkshell> ");
    }
}