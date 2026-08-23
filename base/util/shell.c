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
#include <external/posix/signal.h>
#include <hal/pci.h>
#include <disk/ahci.h>
#include <mm/pmm.h>
#include <disk/ptable.h>
#include <net/net.h>
#include <uacpi/kernel_api.h>
char** gKeEnvp;
int gKeEnvc = 0;

static void KeShlTestLs(const char* path) {
    printf("kernel: ls: Listing for %s\r\n", path);
    VfsDirEntry dirent;
    int idx = 0;
    int handle = OsOpen(path, 0);
    if (handle < 0) { printf("kernel: ls: get handle for path fail.\r\n"); return; }
    while (OsReadDir(handle, &dirent, idx) == 1) {
        const char* Type = (dirent.Type == VFS_TYPE_DIRECTORY) ? "<DIR>" : "     ";
        printf("    %s  %s\r\n", Type, dirent.Name);
        idx++;
    }
    OsClose(handle);
    printf("kernel: ls: total entries %d\r\n", idx);
}

static KSTATUS KeShlPingCallback(NetEthFrameHdr* EFrame, NetIpv4Hdr* Ipv4, NetIcmpEchoHdr* Echo, NetIcmpHdr* Icmp, NetUdpHdr* nouse) {
    printf("%d bytes from %d.%d.%d.%d ttl=%d\r\n", UtilSwapEnd16(Ipv4->Length), Ipv4->Sender[0], Ipv4->Sender[1], Ipv4->Sender[2], Ipv4->Sender[3], Ipv4->Ttl);
    return KSUCCESS;
}

static int KeShlListen = 0;
static KSTATUS KeShlListenCallback(NetEthFrameHdr* EFrame, NetIpv4Hdr* Ipv4, NetIcmpEchoHdr* nouse0, NetIcmpHdr* nouse1, NetUdpHdr* Udp) {
    printf("from %d.%d.%d.%d:%d\r\n", Ipv4->Sender[0], Ipv4->Sender[1], Ipv4->Sender[2], Ipv4->Sender[3], Udp->SrcPort);
    uint8_t* payload = (uint8_t*)((uint64_t)Udp + sizeof(NetUdpHdr));
    for (int i = 0; i < UtilSwapEnd16(Udp->Length); i++) {
        _putchar(payload[i]);
    }
    printf("\r\n");
    KeShlListen = 1;
    return KSUCCESS;
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
        printf("----------------------- misc -----------------------\r\n");
        printf("ver - display kernel version.\r\n");
        printf("echo - echo something to the screen!\r\n");
        printf("ls - list directory\r\n");
        printf("shutdown - does a shutdown using uACPI.\r\n");
        printf("----------------------- process -----------------------\r\n");
        printf("exec - execute a user mode binary.\r\n");
        printf("lsproc - list processes and threads.\r\n");
        printf("kill - kills a process.\r\n");
        printf("----------------------- fs -----------------------\r\n");
        printf("fsisabsol - checks if a path is absolute.\r\n");
        printf("fstranslpath - translates a rel path to a absolute one.\r\n");
        printf("chdir - changes proc cwd.\r\n");
        printf("getcwd - gets current working directory.\r\n");
        printf("stat - posix-compat function that returns a stat struct.\r\n");
        printf("lsdrive - lists mounted drives.\r\n");
        printf("cat - outputs contents of a file.\r\n");
        printf("create - creates a new file.\r\n");
        printf("write - overwrites a file.\r\n");
        printf("----------------------- env -----------------------\r\n");
        printf("getenv - dumps environment variables.\r\n");
        printf("addenv - adds an env variable.\r\n");
        printf("----------------------- pipe -----------------------\r\n");
        printf("iopipe - test IoPipeObj and see if vfs is working with it.\r\n");
        printf("----------------------- memory -----------------------\r\n");
        printf("heapdump - dumps heap regions.\r\n");
        printf("----------------------- cpu/devices -----------------------\r\n");
        printf("cpufeats - cpu features.\r\n");
        printf("lspci - list pci devices.\r\n");
        printf("lsahciport - list ahci ports.\r\n");
        printf("getahci - read from ahci port.\r\n");
        printf("setahci - write a string to an ahci port.\r\n");
        printf("gptdump - checks gpt header and partitions.\r\n");
        printf("lsdrvdev - list devices registered by drivers.\r\n");
        printf("getwalltime - get walltime from a clock device.\r\n");
        printf("ioapiclgirq - translates a legacy irq into ioapic gsi.\r\n");
        printf("----------------------- networking -----------------------\r\n");
        printf("lsnic - list network cards and their relevant info.\r\n");
        printf("netread - waits until a packet comes and reads/dumps packet.\r\n");
        printf("getip - gets our current ip address.\r\n");
        printf("arpreq - request a mac address from an ip address.\r\n");
        printf("ping - pings an ip address.\r\n");
        printf("udplisten - listens on a port for a udp packet.\r\n");
        printf("udptest - tests sending.\r\n");
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
        argv[0] = MmAllocate(strlen(s)+1 * sizeof(char));
        strlcpy(argv[0], s, strlen(s)+1);
        for (int i = 1; i < argc; i++) {
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
        int h = OsOpen(path, 0);
        if (h < 0) {
            printf("OsOpen failed.\r\n");
            MmFree(stat);
            MmFree(path);
            return;
        }
        uint64_t r = KE_SYSCALL_CALL_ARG2(SysFstat, (uint64_t)stat, (uint64_t)stat);
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
        MmFree(stat);
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
    } else if (strcmp(string, "getahci") == 0) {
        printf("enter port num: ");
        char* portstr = KeShlReadStr();
        printf("\r\n");
        int port = AsciiAsInt(portstr);
        MmFree(portstr);
        printf("enter sector num: ");
        char* secstr = KeShlReadStr();
        printf("\r\n");
        int sector = AsciiAsInt(secstr);
        MmFree(secstr);
        printf("enter number of sectors: ");
        char* secnumstr = KeShlReadStr();
        printf("\r\n");
        int sectornum = AsciiAsInt(secnumstr);
        MmFree(secnumstr);        
        uint8_t* buffer = PmmAllocate();
        uint8_t* vbuf = (uint8_t*)P2V(buffer);
        if (!buffer) {
            printf("failed to allocate page for read.\r\n");
            return;
        }
        memset((void*)vbuf, 0, PAGE_SIZE);
        KSTATUS r = AhciPortRead(AhciGetPort(port), sector, sectornum, buffer);
        if (r == KSUCCESS) {
            printf("successfully read from port.\r\n");
            printf("dumping first 1024 bytes: \r\n");
            for (int i = 0; i < 1024; i++) {
                _putchar(vbuf[i]);
            }
            printf("\r\nfinished dump\r\n");
        } else if (r == KFAIL) {
            printf("failed to read from port.\r\n");
            PmmFree(buffer);
            return;
        } else if (r == KHUNG) {
            printf("port is hung or being used.\r\n");
            PmmFree(buffer);
            return;
        }
    } else if (strcmp(string, "setahci") == 0) {
        printf("enter port num: ");
        char* portstr = KeShlReadStr();
        printf("\r\n");
        int port = AsciiAsInt(portstr);
        MmFree(portstr);
        printf("enter sector num: ");
        char* secstr = KeShlReadStr();
        printf("\r\n");
        int sector = AsciiAsInt(secstr);
        MmFree(secstr);
        printf("enter number of sectors: ");
        char* secnumstr = KeShlReadStr();
        printf("\r\n");
        int sectornum = AsciiAsInt(secnumstr);
        MmFree(secnumstr);        
        uint8_t* buffer = PmmAllocate();
        uint8_t* vbuf = (uint8_t*)P2V(buffer);
        if (!buffer) {
            printf("failed to allocate page for write.\r\n");
            return;
        }
        memset((void*)vbuf, 0, PAGE_SIZE);
        printf("type string to write to port: ");
        char* wrstr = KeShlReadStr();
        printf("\r\n");
        memcpy((void*)vbuf, wrstr, strlen(wrstr));
        MmFree(wrstr);
        KSTATUS r = AhciPortWrite(AhciGetPort(port), sector, sectornum, (const void*)buffer);
        if (r == KSUCCESS) {
            printf("successfully wrote to port.\r\n");
        } else if (r == KFAIL) {
            printf("failed to write to port.\r\n");
            PmmFree(buffer);
            return;
        } else if (r == KHUNG) {
            printf("port is hung or being used.\r\n");
            PmmFree(buffer);
            return;
        }
    } else if (strcmp(string, "gptdump") == 0) {
        uint8_t* buffer = PmmAllocate();
        uint8_t* vbuf = (uint8_t*)P2V(buffer);
        memset(vbuf, 0, 4096);
        KSTATUS r = AhciPortRead(AhciGetPort(0), 1, 1, buffer);
        if (r == KFAIL) {
            printf("failed to write to port.\r\n");
            PmmFree(buffer);
            return;
        } else if (r == KHUNG) {
            printf("port is hung or being used.\r\n");
            PmmFree(buffer);
            return;
        }
        KSTATUS r2 = PtableEnumerate((void*)vbuf);
        if (r2 != KSUCCESS) {
            printf("failed to call PtableEnumerate.\r\n");
            PmmFree(buffer);
            return;
        }
    } else if (strcmp(string, "lsdrive") == 0) {
        VfsListMountedDrives();
    } else if (strcmp(string, "cat") == 0) {
        printf("enter path: ");
        char* path = KeShlReadStr();
        printf("\r\n");
        int h = OsOpen(path, 0);
        if (h < 0) {
            printf("failed to open file.\r\n");
            MmFree(path);
            return;
        } else {
            int fsz = OsGetFileSize(h);
            char* buf = MmAllocate(fsz);
            memset(buf, 0, fsz);
            int r = OsRead(h, buf, fsz);
            for (int i = 0; i < fsz; i++) _putchar(buf[i]);
            printf("\r\n");
            OsClose(h);
            MmFree(buf);
        }
        MmFree(path);
    } else if (strcmp(string, "create") == 0) {
        printf("enter path: ");
        char* path = KeShlReadStr();
        printf("\r\n");
        int r = OsCreate(path, VFS_TYPE_FILE);
        if (r == -1) {
            printf("fail.\r\n");
        }
    } else if (strcmp(string, "write") == 0) {
        printf("enter path: ");
        char* path = KeShlReadStr();
        printf("\r\n");
        int h = OsOpen(path, 0);
        if (h < 0) {
            printf("failed to open file.\r\n");
            return;
        }
        printf("enter string to overwrite: ");
        char* newcontent = KeShlReadStr();
        printf("\r\n");
        int r = OsWrite(h, newcontent, strlen(newcontent));
        if (r < 0) printf("OsWrite call failed.\r\n");
        OsClose(h);
        MmFree(path);
        MmFree(newcontent);
    } else if (strcmp(string, "lsdrvdev") == 0) {
        KeListDevices();
        KeListDrivers();
    } else if (strcmp(string, "getwalltime") == 0) {
        printf("enter device name: ");
        char* devname = KeShlReadStr();
        printf("\r\n");
        KeDeviceObj* Dev = KeFindDeviceByName(devname);
        if (!Dev) {
            printf("no such device '%s'\r\n", devname);
            MmFree(devname);
            return;
        }
        KeDevClockWallTime Walltime;
        KeIoRequest Irp;
        Irp.Major = IO_HWSPEC + KE_WALLTIME_HWSPEC_OFF;
        Irp.Buffer = &Walltime;
        Irp.Length = sizeof(KeDevClockWallTime);
        Irp.ReadBytes = 0;
        KSTATUS r = KeIoDispatch(Dev, &Irp);
        if (r != KSUCCESS) {
            printf("failed to do iorequest to device. (KSTATUS 0x%lx)\r\n", r);
            MmFree(devname);
            return;
        }
        printf("%d:%d:%d\r\n", Walltime.Hours, Walltime.Minutes, Walltime.Seconds);
        printf("%d/%d/%d\r\n", Walltime.Days, Walltime.Month, Walltime.Year);
        printf("\r\n");
    } else if (strcmp(string, "fstranslpath") == 0) {
        printf("enter path: \r\n");
        char* path = KeShlReadStr();
        char acpath[VFS_MAX_ALLOWED_PATH];
        VfsTranslatePath(path, acpath, ThrGetCurrent()->ParentProc);
        printf("translated path: %s\r\n", acpath);
        MmFree(path);
    } else if (strcmp(string, "kill") == 0) {
        printf("enter pid: ");
        char* pids = KeShlReadStr();
        printf("\r\n");
        int pid = AsciiAsInt(pids);
        MmFree(pids);
        printf("enter signum: ");
        char* sigs = KeShlReadStr();
        printf("\r\n");
        int sig = AsciiAsInt(sigs);
        MmFree(sigs);
        ProcessCtrlBlk* process = ProcFindByPid(pid, KernelGetInformation());
        ThreadCtrlBlk* current = process->ThreadListHead;
        while (current != NULL) {
            current->SigPendingSet |= (1ULL << sig);
            current = current->ProcNext;
        }
        printf("sent signal to pid.\r\n");
    } else if (strcmp(string, "lsnic") == 0) {
        NetInterface* Nic = NetGetLinkedList();
        while (Nic != NULL) {
            printf("%s: \r\n", Nic->Name);
            printf("    MAC Address: ");
            UtilPrintMacAddr(Nic->MacAddress);
            printf(" \r\n");
            printf("    Drvdev info: \r\n");
            printf("         name='%s' driver name='%s'\r\n", Nic->Device->Name, Nic->Device->Owner->Name);
            Nic = Nic->Next;
        }
    } else if (strcmp(string, "ioapiclgirq") == 0) {
        // ...
    } else if (strcmp(string, "shutdown") == 0) {
        KSTATUS r = AcpiSystemShutdown();
        if (r != KSUCCESS) {
            printf("failed to do shutdown. KSTATUS %d\r\n", r);
        }
        // unreachable
    } else if (strcmp(string, "netread") == 0) {
        NetInterface* Nic = NetGetLinkedList();
        if (!Nic) {
            printf("error: no NIC in system.\r\n");
            return;
        } else {
            uint8_t* Buffer = MmAllocate(1500);
            memset((void*)Buffer, 0, 1500);
            uint16_t BytesRead;
            KSTATUS r = NetReadRaw(Nic, Buffer, 1500, &BytesRead);
            if (r != KSUCCESS) {
                printf("failed. KSTATUS 0x%lx\r\n", r);
                MmFree(Buffer);
                return;
            }
            printf("read %d bytes.\r\n", BytesRead);
            NetEthFrameHdr* EthFrame = (NetEthFrameHdr*)Buffer;
            printf("Packet from [");
            UtilPrintMacAddr(EthFrame->SrcMac);
            printf("] to [");
            UtilPrintMacAddr(EthFrame->DestMac);
            printf("]\r\n");

            MmFree(Buffer);
        }
    } else if (strcmp(string, "getip") == 0) {
        printf("Current IP Address: [%d.%d.%d.%d]\r\n", KernelGetInformation()->net.Ip[0], KernelGetInformation()->net.Ip[1], KernelGetInformation()->net.Ip[2], KernelGetInformation()->net.Ip[3]);
        printf("Router IP Address: [%d.%d.%d.%d]\r\n", KernelGetInformation()->net.RouterIp[0], KernelGetInformation()->net.RouterIp[1], KernelGetInformation()->net.RouterIp[2], KernelGetInformation()->net.RouterIp[3]);
    } else if (strcmp(string, "arpreq") == 0) {
        printf("enter ip[0]: ");
        char* ip0s = KeShlReadStr();
        printf("\r\n");
        uint8_t ip0 = AsciiAsInt(ip0s);
        printf("enter ip[1]: ");
        char* ip1s = KeShlReadStr();
        printf("\r\n");
        uint8_t ip1 = AsciiAsInt(ip1s);
        printf("enter ip[2]: ");
        char* ip2s = KeShlReadStr();
        printf("\r\n");
        uint8_t ip2 = AsciiAsInt(ip2s);
        printf("enter ip[3]: ");
        char* ip3s = KeShlReadStr();
        printf("\r\n");
        uint8_t ip3 = AsciiAsInt(ip3s);
        uint8_t ip[4] = {ip0, ip1, ip2, ip3};
        NetArpEntry* Arp = NetArpTableResolve(&KernelGetInformation()->net.ArpHead, ip);
        if (Arp != NULL) {
            printf("resolved ip to mac [");
            UtilPrintMacAddr(Arp->Mac);
            printf("]\r\n");
        }
        MmFree(ip3s);
        MmFree(ip2s);
        MmFree(ip1s);
        MmFree(ip0s);
    } else if (strcmp(string, "ping") == 0) {
        printf("enter ip[0]: ");
        char* ip0s = KeShlReadStr();
        printf("\r\n");
        uint8_t ip0 = AsciiAsInt(ip0s);
        printf("enter ip[1]: ");
        char* ip1s = KeShlReadStr();
        printf("\r\n");
        uint8_t ip1 = AsciiAsInt(ip1s);
        printf("enter ip[2]: ");
        char* ip2s = KeShlReadStr();
        printf("\r\n");
        uint8_t ip2 = AsciiAsInt(ip2s);
        printf("enter ip[3]: ");
        char* ip3s = KeShlReadStr();
        printf("\r\n");
        uint8_t ip3 = AsciiAsInt(ip3s);
        uint8_t ip[4] = {ip0, ip1, ip2, ip3};
        printf("PING %d.%d.%d.%d\r\n", ip[0], ip[1], ip[2], ip[3]);
        NetCallback callback;
        callback.CallBack = KeShlPingCallback;
        callback.Type = NET_CALLBACK_ICMP;
        NetRegisterCallback(0, &callback);
        for (uint16_t i = 0; i < 10; i++) {
            KSTATUS r = NetIcmpEchoRequest(NetGetLinkedList(), ip, i);
            printf("sent ping. seq=%d kstatus=0x%lx\r\n", i, r);
            uacpi_kernel_sleep(1000); // using uacpi api for this is mad work
        }
        MmFree(ip3s);
        MmFree(ip2s);
        MmFree(ip1s);
        MmFree(ip0s);
        NetDeregisterCalback(0);
    } else if (strcmp(string, "udplisten") == 0) {
        printf("enter port [0-65535]: ");
        char* ports = KeShlReadStr();
        printf("\r\n");
        uint16_t port = AsciiAsInt(ports);
        MmFree(ports);
        NetCallback callback;
        callback.Port = port;
        callback.Type = NET_CALLBACK_UDP;
        callback.CallBack = KeShlListenCallback;
        NetRegisterCallback(1, &callback);
        while (!KeShlListen);
        NetDeregisterCalback(1);
    } else if (strcmp(string, "udptest") == 0) {
        uint8_t* buf = MmAllocate(13);
        strlcpy(buf, "Hello world from ah5kos!", 13);
        uint8_t ipaddr[4] = {192, 168, 100, 2}; // ip of host on tap
        uint16_t port = 25565; // only port i could think of, prob bcz of minecraft
        KSTATUS r = NetUdpSend(NetGetLinkedList(), ipaddr, buf, 13, 1234, port); // from us:1234 -> 192.168.100.2:25565
        printf("KSTATUS of UdpSend 0x%lx\r\n", r);
        MmFree(buf);
    }
    else {
        if (strcmp(string, "") != 0) printf("error: no such command '%s' \r\n", string);
    }
}
extern uint64_t PmmTotalFreePhysRam;

void KeUtilShell() {
    FbClear();
    printf("kshell: Welcome to ah5kos 1.0.0\r\n");
    printf("kshell: Total Free Physical RAM: %ld MB\r\n", UTIL_DIV_RUP(UTIL_DIV_RUP(PmmTotalFreePhysRam, 1024), 1024));
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