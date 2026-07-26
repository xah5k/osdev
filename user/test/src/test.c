#include <stdint.h>

extern void exit(uint64_t code);
extern uint64_t kill(uint64_t pid);
extern uint64_t spawn(const char* path);
extern uint64_t conwrite(const char* thing);
extern uint64_t yield(uint64_t pad);
void AppMain(const char* argv[], int argc) {
    volatile int x = 0;
    while (1) {
        x++;
    }
    exit(0);
}