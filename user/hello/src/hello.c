#include <sys/mman.h>
#include <stdio.h>

int main() {
    // sm shi to test mmap
    printf("mmap test start\r\n");

    void *a = mmap(NULL, 4096,   PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
    void *b = mmap(NULL, 8192,   PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
    void *c = mmap(NULL, 4096,   PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);

    printf("a=%p b=%p c=%p\r\n", a, b, c);
    ((char*)a)[0] = 1;
    ((char*)b)[8191] = 2;
    ((char*)c)[0] = 3;
    munmap(b, 8192);
    printf("a[0]=%d c[0]=%d\r\n", ((char*)a)[0], ((char*)c)[0]);
    void *d = mmap(NULL, 16384, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
    printf("d=%p\r\n", d);
    ((char*)d)[16383] = 4;
    printf("d[16383]=%d (expect 4)\r\n", ((char*)d)[16383]);
    munmap(a, 4096);
    munmap(d, 16384);
    munmap(c, 4096);
    printf("mmap test done\r\n");
    return 0;
}