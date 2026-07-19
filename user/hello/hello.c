

void AppMain() {
    void(*putchar)(char) = (void(*)(char))0xffffffffffe06833;
    putchar('H');
    putchar('i');
    putchar('\r');
    putchar('\n');
}