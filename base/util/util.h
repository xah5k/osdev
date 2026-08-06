#pragma once
#include <stdint.h>
typedef unsigned short wchar_t;

int strcmp(const char* s1, const char* s2);
int strlen(const char* s);
int oct2bin(unsigned char *str, int size);
int strcmpl(const char* s1, const char* s2, int max_len);
const char *basename(const char *path);
int AsciiAsInt(const char* str);
void strlcpy(char* dst, const char* src, uint64_t dstsize);
void UtilPrintW(wchar_t* ptr, int len);