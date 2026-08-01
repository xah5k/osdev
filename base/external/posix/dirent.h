#pragma once
/* Copyright (C) 1996-2025 Free Software Foundation, Inc. */
typedef struct {
    unsigned long int d_ino;
    long int d_off;
    unsigned short int d_reclen;
    unsigned char d_type;
    char d_name[256];
} posixdirent;
#define DT_UNKNOWN  0
#define DT_REG      8
#define DT_DIR      4