

void AppMain() {
    void(*putchar)(char) = (void(*)(char))0xffffffffffe05374;
    putchar('H');
    putchar('i');
    putchar('\r');
    putchar('\n');
}