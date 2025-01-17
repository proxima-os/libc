#include <stdio.h>

int main(int argc, char *argv[]) {
    printf("Hello, World! Currently running from an executable that was linked dynamically by the dynamic linker.\n");
    printf("argc = %d\n", argc);

    for (int i = 0; i < argc; i++) {
        printf(" %d. %s\n", i, argv[i]);
    }
}

__attribute__((used)) void _init(void) {
}

__attribute__((used)) void _fini(void) {
}
