#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include "window.h"
#include <unistd.h>
#include <string.h>
#include "../../shared/dwmapi.h"
#include "ah5kos.h"
#include <time.h>

Server* gServer = NULL;

#define RANDRANGE(min, max) min + rand() % (max - min + 1)

Window* Server::CreateWindow(std::string wname, int w, int h, int x, int y) {
    Window* window =  new Window(wname, w, h, this->dwm, x, y);
    this->dwm->RegisterWindow((const Window*)window);
    return window;
}

extern "C" void SignalReceive(int SigIdx) {
    if (!gServer) return;
    OsMessage* m = OsMsgGet(NULL, sizeof(DwmPacket) + gServer->dwm->GetGlobalBuffer()->size);
    if (!m) {
        printf("dwm: server: failed to get message from queue.\r\n");
        return;
    }
    DwmPacket* pck = (DwmPacket*)((uint64_t)m + sizeof(OsMessage));
    switch (pck->Type) {
        case DWMPCK_CRWIN: {
            Window* w = gServer->CreateWindow(std::to_string(m->FromPid), pck->CrWinWidth, pck->CrWinHeight, RANDRANGE(0, gServer->dwm->GetFrontBuffer()->width), RANDRANGE(0, gServer->dwm->GetFrontBuffer()->height));
            w->OwningPid = m->FromPid;
            OsMessage* msg = gServer->CreateFbInfoMsg(w->GetFb(), (WNDHDL)w->Wid);
            msg->ToPid = m->FromPid;
            DwmPacket* pck2 = (DwmPacket*)((uint64_t)msg + sizeof(OsMessage));
            KSTATUS s = OsSharedMapB(m->FromPid, (void*)w->GetFb()->ptr, (uint64_t)w->GetFb()->size);
            if (s != KSUCCESS) printf("dwm: server: mapping fb into pids process failed! KSTATUS 0x%x\r\n", s);
            s = OsMsgSend(msg);
            free(m);
            free(msg);
            gServer->dwm->Redraw();
            return;
        }
        case DWMPCK_DRAW: {
            WNDHDL* pWhdl = (WNDHDL*)((uint64_t)pck + sizeof(DwmPacket));
            std::list<Window*>& wlist = gServer->dwm->GetWindowsList();
            for (Window* w : wlist) {
                if (w->Wid == *pWhdl) {
                    w->FullWindowRedraw = true;
                    gServer->dwm->Redraw();
                }
            }
            break;
        }
        default: {
            printf("dwm: server: invalid pck type %d.\r\n", pck->Type);
            break;
        }
    }
    free(m);
    return;
}

OsMessage* Server::CreateFbInfoMsg(Framebuffer* info, WNDHDL whdl) {
    OsMessage* m = (OsMessage*)malloc(sizeof(OsMessage) + sizeof(DwmPacket) + sizeof(DwmCrWinResponse));
    m->FromPid = getpid();
    // to pid should be set by caller
    m->Length = sizeof(DwmPacket) + sizeof(DwmCrWinResponse);
    DwmPacket* p = (DwmPacket*)((uint64_t)m + sizeof(OsMessage));
    p->Type = DWMPCK_REFBINFO;
    p->CrWinHeight = 0;
    p->CrWinWidth = 0;
    DwmCrWinResponse* resp = (DwmCrWinResponse*)((uint64_t)p + sizeof(DwmPacket));
    Framebuffer* f = &resp->fbinf;
    memcpy((void*)f, (void*)info, sizeof(Framebuffer));
    resp->WindowHandle = whdl;
    // printf("whdl=0x%lx resp->WindowHandle=0x%lx resp=0x%lx f=0x%lx.\r\n", whdl, resp->WindowHandle, resp, f);
    return m;
}

Server::Server(Dwm* dwm) {
    this->dwm = dwm;
    gServer = this;
    srand(time(nullptr));
    uint64_t r = dosyscall(36, SIGNAL_RECEIVEMSG, (uint64_t)SignalReceive, 0, 0, 0);
    printf("dwm: server: registered signal.\r\n");
}

Server::~Server() {}

