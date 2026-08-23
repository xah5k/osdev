#pragma once
#include <stddef.h>
#include <stdint.h>
#define UTIL_DIV_RUP(a, b) (a + b - 1) / b

typedef struct {
    uint64_t x;
    uint64_t y;
} Point;

int strcmp(const char* s1, const char* s2);
int strlen(const char* s);
int oct2bin(unsigned char *str, int size);
int strcmpl(const char* s1, const char* s2, int max_len);
const char *basename(const char *path);
int AsciiAsInt(const char* str);
void strlcpy(char* dst, const char* src, uint64_t dstsize);
void UtilPrintW(wchar_t* ptr, int len);
uint16_t UtilSwapEnd16(uint16_t n);
uint32_t UtilSwapEnd32(uint32_t n);
void UtilPrintMacAddr(uint8_t* macaddr);
void UtilPrintFmtAt(const char* message, int x, int y, ...);