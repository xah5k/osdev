#include <util/util.h>
#include <kedriver.h>

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