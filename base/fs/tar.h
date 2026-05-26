#pragma once
#include <stdint.h>

typedef struct {
    char Filename[100];
    char Filemode[8];
    char OUID[8];
    char GID[8];
    char Size[12];
    char ModTime[12];
    char Checksum[8];
    char Flag;
    char Linkedname[100];
    char Indicator[6];
    char UstarVer[2];
    char OUsername[32];
    char OGroupname[32];
    char DevMaj[8];
    char DevMin[8];
    char Prefix[155];
    char Padding[12];
} __attribute__((packed)) TarFileEntry;


TarFileEntry* TarFsLookup(uint8_t* archive, char* filename);

void TarInitalizeVfs(void* archive);