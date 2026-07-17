

void AppMain() {
    void(*putchar)(char) = (void(*)(char))0xffffffffffe0573d;
    putchar('H');
    putchar('i');
    putchar('\r');
    putchar('\n');
}