

void AppMain() {
    void(*putchar)(char) = (void(*)(char))0xffffffffffe055e3;
    putchar('H');
    putchar('i');
    putchar('\r');
    putchar('\n');
}