extern int main(const char**, int);
#include "syscall.h"
__attribute__((section(".text._start")))
void _start(const char** argv, int argc) {
    int r = main(argv, argc);
    exit(r);
}