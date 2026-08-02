#include <stdio.h>
#include <unistd.h>

int main(int argc, const char* argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("before fork: pid-ish test, argc=%d\r\n", argc);

    int local_var = 100;

    int result = fork();

    if (result == 0) {
        // Child
        local_var += 1;
        printf("child: fork() returned %d, local_var=%d\r\n", result, local_var);
    } else if (result > 0) {
        // Parent
        local_var += 1000;
        printf("parent: fork() returned %d (child pid), local_var=%d\r\n", result, local_var);
    } else {
        printf("fork() failed!\r\n");
        return 1;
    }

    printf("done, my local_var=%d\r\n", local_var);

    while (1) {
        for (volatile int i = 0; i < 100000000; i++);
        printf("%s still alive, local_var=%d\r\n", result == 0 ? "child" : "parent", local_var);
    }

    return 0;
}