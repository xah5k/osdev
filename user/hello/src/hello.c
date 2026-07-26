#include <stdint.h>

extern void exit(uint64_t code);
extern uint64_t kill(uint64_t pid);
extern uint64_t spawn(const char* path, const char** argv, int argc);
extern uint64_t conwrite(const char* thing);
extern uint64_t yield(uint64_t pad);
void AppMain(const char* argv[], int argc) {
    conwrite("hi from program!\r\n");
    conwrite("argv[0] = ");
    conwrite(argv[0]);
    spawn("initrd:/programs/test.elf", 0, 0);
    exit(0);
}