#pragma once

typedef enum {
    KEY_F1 = 0xF0,
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F8,
    KEY_F9,
    KEY_F10,
    KEY_NUMLCK,
    KEY_SCRLCK,
    KEY_NM7 = '7',
    KEY_NM8,
    KEY_NM9,
    KEY_NMMINUS = '-',
    KEY_NM4 = '4',
    KEY_NM5,
    KEY_NM6,
    KEY_NMPLUS = '+',
    KEY_NM1 = '1',
    KEY_NM2 = '2',
    KEY_NM3 = '3',
    KEY_NM0 = '0',
    KEY_NMDOT = '.',
    KEY_F11 = 252,
    KEY_F12
} KbdTranslExtraKey;

char KbdTranslGetc();