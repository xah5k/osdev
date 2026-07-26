#include <stdint.h>

extern void exit(uint64_t code);
extern uint64_t kill(uint64_t pid);
extern uint64_t spawn(const char* path, const char** argv, int argc);
extern uint64_t conwrite(const char* thing);
extern uint64_t yield(uint64_t pad);
extern uint64_t sbrk(uint64_t inc);
static uint64_t heap_ptr = 0;
static uint64_t heap_end = 0;

void* malloc(uint64_t size) {
    size = (size + 15) & ~15; // 16-byte align
    if (heap_ptr + size > heap_end) {
        uint64_t grow = size > 0x10000 ? size : 0x10000; // grow in chunks
        uint64_t old_break = sbrk(grow);
        if (heap_ptr == 0) heap_ptr = old_break;
        heap_end = old_break + grow;
    }
    void* result = (void*)heap_ptr;
    heap_ptr += size;
    return result;
}

void AppMain(const char* argv[], int argc) {
    conwrite("hi from program!\r\n");
    conwrite("argv[0] = ");
    conwrite(argv[0]);
    spawn("initrd:/programs/test.elf", 0, 0);
    conwrite("test sbrk using bump malloc.\r\n");
    for (int i = 0; i < 1024; i++) {
        void* x= malloc(1024);
    }
    exit(0);
}