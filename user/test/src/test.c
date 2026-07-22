#include <stdint.h>

extern void exit(uint64_t code);
extern uint64_t kill(uint64_t pid);
void AppMain() {
    kill(1);
    exit(0);
}