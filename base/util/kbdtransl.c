#include "kbdtransl.h"
#include <kedriver.h>
#include <mm/heap.h>
static int gStateShift = 0;
static int gStateCapsLock = 0;
static int gStateCtrl = 0;

static const char ScancodeToAsciiLower[] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t','q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\r',
    0,
    'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,
    '\\','z','x','c','v','b','n','m',',','.','/',
    0,
    '*',
    0,
    ' ',
    0, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10,
    KEY_NUMLCK, KEY_SCRLCK,
    KEY_NM7, KEY_NM8, KEY_NM9,
    KEY_NMMINUS,
    KEY_NM4, KEY_NM5, KEY_NM6,
    KEY_NMPLUS,
    KEY_NM1, KEY_NM2, KEY_NM3,
    KEY_NM0, KEY_NMDOT,
    0, 0, 0,
    KEY_F11, KEY_F12
};

static const char ScancodeToAsciiUpper[] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t','Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\r',
    0,
    'A','S','D','F','G','H','J','K','L',':','"','~',
    0,
    '|','Z','X','C','V','B','N','M','<','>','?',
    0,
    '*',
    0,
    ' ',
    0, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10,
    KEY_NUMLCK, KEY_SCRLCK,
    KEY_NM7, KEY_NM8, KEY_NM9,
    KEY_NMMINUS,
    KEY_NM4, KEY_NM5, KEY_NM6,
    KEY_NMPLUS,
    KEY_NM1, KEY_NM2, KEY_NM3,
    KEY_NM0, KEY_NMDOT,
    0, 0, 0,
    KEY_F11, KEY_F12
};

#define SC_LSHIFT 0x2A
#define SC_RSHIFT 0x36
#define SC_LSHIFT_REL (0x2A | 0x80)
#define SC_RSHIFT_REL (0x36 | 0x80)
#define SC_CAPSLOCK 0x3A
#define SC_ENTER 0x1C

#define SC_CTRL 0x1D
#define SC_CTRL_REL (0x1D | 0x80)

char KbdTranslScancode(uint8_t scancode) {
    int release = scancode & 0x80;
    uint8_t code = scancode & 0x7F;
    if (code == SC_LSHIFT || code == SC_RSHIFT) {
        gStateShift = !release;
        return 0;
    }
    if (code == SC_CTRL) {
        gStateCtrl = !release;
        return 0;
    }
    if (code == SC_CAPSLOCK && release) {
        gStateCapsLock = !gStateCapsLock;
        return 0;
    }
    if (code == SC_ENTER && !release) {
        return '\n';
    }
    if (release) return 0;
    if (code >= 128) return 0;

    int UseUpper = gStateShift;
    if (gStateCapsLock && code >= 0x10 && code <= 0x32) {
        UseUpper = !UseUpper;
    }
    char c = UseUpper ? ScancodeToAsciiUpper[code] : ScancodeToAsciiLower[code];
    if (gStateCtrl && c >= 'a' && c <= 'z') {
        return c - 'a' + 1;
    }
    if (gStateCtrl && c >= 'A' && c <= 'Z') {
        return c - 'A' + 1;
    }
    return c;
}

char KbdTranslGetc() {
    uint8_t scancode;
    KeDeviceObj* dev = KeFindDeviceByName("ps2kbd");
    KeIoRequest stackirp;
    KeIoRequest* irp = &stackirp;
    irp->Major = IO_READ;
    irp->Buffer = &scancode;
    irp->Length = 1;
    KeIoDispatch(dev, irp);
    return KbdTranslScancode(scancode);
}