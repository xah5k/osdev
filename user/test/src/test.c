#include <stdint.h>
#include "../../../libs/syscall.h"
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


int main(const char* argv[], int argc) {
    conwrite("test.elf: test sbrk using bump malloc.\r\n");
    for (int i = 0; i < 1024; i++) {
        void* x= malloc(1024);
    }
    return 0;
}