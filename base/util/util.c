#include <util/util.h>
#include <kedriver.h>
#include <memory.h>
#include <printfwrapper.h>

int oct2bin(unsigned char *str, int size) {
    int n = 0;
    unsigned char *c = str;
    while (size-- > 0) {
        n *= 8;
        n += *c - '0';
        c++;
    }
    return n;
}
KE_EXPORT_SYMBOL(oct2bin);

int strlen(const char* s) {
    int x = 0;
    while (*s != '\0') {
        x++;
        s++;
    }
    return x;
}
KE_EXPORT_SYMBOL(strlen);

int strcmp(const char* s1, const char* s2) {
    int i = 0;
    while (s1[i] != '\0' && s2[i] != '\0') {
        if (s1[i] != s2[i]) return (unsigned char)s1[i] - (unsigned char)s2[i];
        i++;
    }
    return (unsigned char)s1[i] - (unsigned char)s2[i];
}
KE_EXPORT_SYMBOL(strcmp);

int strcmpl(const char* s1, const char* s2, int max_len) {
    for (int i = 0; i < max_len; i++) {
        if (s1[i] != s2[i]) return 1;
        if (s1[i] == '\0')  return 0;
    }
    return 0;
}
KE_EXPORT_SYMBOL(strcmpl);

const char *basename(const char *path) {
    uint64_t len = strlen(path);
    if (len == 0) {
        return path;
    }
    for (uint64_t i = len; i > 0; i--) {
        if (path[i - 1] == '/') {
            return &path[i];
        }
    }

    return path;
}

int is_digit(char ch) {
    return (ch >= '0') && (ch <= '9');
}

int AsciiAsInt(const char* str) {
    unsigned int i = 0U;
    while (is_digit(*str)) {
        i = i * 10U + (unsigned int)(*((str)++) - '0');
    }
    return i;
}

// cuz later im prob gna use this for more file related shi
void strlcpy(char* dst, const char* src, uint64_t dstsize) {
    uint64_t len = strlen(src);
    if (len > dstsize - 1) len = dstsize - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}


// prints a CHAR16 string
// note: assumes the chars are within 0-127
void UtilPrintW(wchar_t* ptr, int len) {
    char* p = (char*)ptr;
    for (int i = 0; i < len; i++) {
        _putchar(p[i]);
    }
}