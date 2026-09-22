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

static void ServerProcessMsg(OsMessage* m) {
    DwmPacket* pck = (DwmPacket*)((uint64_t)m + sizeof(OsMessage));
    switch (pck->Type) {
        case DWMPCK_CRWIN: {
            Window* w = gServer->CreateWindow(std::to_string(m->FromPid), pck->CrWinWidth, pck->CrWinHeight, CLAMP(pck->CrWinX, 0, gServer->dwm->GetFrontBuffer()->width), CLAMP(pck->CrWinY, 0, gServer->dwm->GetFrontBuffer()->height));
            w->OwningPid = m->FromPid;
            w->Options = pck->CrWinOpt;
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
                    // printf("window with whdnl 0x%lx from pid %d {message fpid = %d} has completed drawing. wname=%s\r\n", *pWhdl, w->OwningPid, m->FromPid, w->GetName().c_str());
                    w->FullWindowRedraw = true;
                    break;
                }
            }
            break;
        }
        case DWMPCK_CHNAME: {
            WNDHDL* pWhdl = (WNDHDL*)((uint64_t)pck + sizeof(DwmPacket));
            const char* pName = (const char*)((uint64_t)pWhdl + sizeof(WNDHDL));
            // printf("dwm: server: req to change name of whdl 0x%lx to %s\r\n", *pWhdl, pName);
            std::list<Window*>& wlist = gServer->dwm->GetWindowsList();
            for (Window* w : wlist) {
                if (w->Wid == *pWhdl) {
                    w->ChangeName(pName);
                    w->DecorationRedraw = true;
                    break;
                }
            }
            break;
        }
        default: {
            printf("dwm: server: invalid pck type %d.\r\n", pck->Type);
            break;
        }
    }
}
extern "C" void SignalReceive(int SigIdx) {
    if (!gServer) return;
    OsMessage* m;
    while ((m = OsMsgGet(NULL, sizeof(DwmPacket) + gServer->dwm->GetGlobalBuffer()->size)) != NULL) {
        if (!m) {
            printf("dwm: server: failed to get message from queue.\r\n");
            return;
        }
        ServerProcessMsg(m);
        // free(m);
    }
    return;
}

KSTATUS Server::SendEventMsg(uint64_t ToPid, DwmReEventResponse* r) {
    OsMessage* m = (OsMessage*)malloc(sizeof(OsMessage) + sizeof(DwmPacket) + sizeof(DwmReEventResponse));
    memset((void*)m, 0, sizeof(OsMessage));
    m->FromPid = getpid();
    m->Length = sizeof(DwmPacket) + sizeof(DwmReEventResponse);
    m->ToPid = ToPid;
    DwmPacket* p = (DwmPacket*)((uint64_t)m + sizeof(OsMessage));
    p->Type = DWMPCK_REEVENT;
    p->CrWinHeight = 0;
    p->CrWinWidth = 0;
    DwmReEventResponse* resp = (DwmReEventResponse*)((uint64_t)p + sizeof(DwmPacket));
    memcpy((void*)resp, r, sizeof(DwmReEventResponse));
    KSTATUS s;
    s = OsMsgSend(m);
    free(m);
    return s;
}

OsMessage* Server::CreateFbInfoMsg(Framebuffer* info, WNDHDL whdl) {
    OsMessage* m = (OsMessage*)malloc(sizeof(OsMessage) + sizeof(DwmPacket) + sizeof(DwmCrWinResponse));
    memset((void*)m, 0, sizeof(OsMessage));
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

