#include <stdint.h>

extern void exit(uint64_t code);
extern uint64_t kill(uint64_t pid);
extern uint64_t spawn(const char* path);
void AppMain() {
    spawn("initrd:/programs/test.elf");
    exit(0);
}