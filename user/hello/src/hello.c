#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

void print_cwd(const char* label) {
    char buf[256];
    if (getcwd(buf, sizeof(buf))) {
        printf("%s: cwd = %s\r\n", label, buf);
    } else {
        printf("%s: getcwd failed!\r\n", label);
    }
}

int main(int argc, const char* argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);

    print_cwd("start");

    // 1. chdir into a real directory
    if (chdir("initrd:/drivers") == 0) {
        printf("chdir to initrd:/drivers: success\r\n");
    } else {
        printf("chdir to initrd:/drivers: FAILED (unexpected)\r\n");
    }
    print_cwd("after chdir drivers");

    // 2. chdir into a file — should fail
    if (chdir("initrd:/programs/hello.elf") == 0) {
        printf("chdir to hello.elf: succeeded (BUG - should have failed)\r\n");
    } else {
        printf("chdir to hello.elf: correctly failed\r\n");
    }
    print_cwd("after failed chdir attempt"); // should be unchanged from step 1

    // 3. chdir into a nonexistent path — should fail
    if (chdir("initrd:/this/does/not/exist") == 0) {
        printf("chdir to bogus path: succeeded (BUG)\r\n");
    } else {
        printf("chdir to bogus path: correctly failed\r\n");
    }
    print_cwd("after bogus chdir attempt"); // should still be unchanged

    // 4. relative chdir, if your cwd supports it (e.g. ".." if implemented)
    if (chdir("..") == 0) {
        print_cwd("after chdir ..");
    } else {
        printf("chdir .. failed (may be expected if .. not yet supported)\r\n");
    }

    return 0;
}