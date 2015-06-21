#include <ctype.h>
#include <stdio.h>
#include <stdint.h>

void fn_22b(uint8_t *mem, int start) {
    int i;
    uint8_t A;

    A = mem[start];
    for (i = start+1; i < 16; ++i) {
        A = (A + 1) % 16;
        A = (A + mem[i]) % 16;
        mem[i] = A;
    }
}

void inverse_22b(uint8_t *mem, int start) {
    int i;
    uint8_t A, nextA;

    A = mem[start];
    nextA = A;
    for (i = start+1; i < 16; ++i) {
        nextA = mem[i];
        mem[i] -= (A + 1);
        if (mem[i] > 16)
            mem[i] += 16;
        A = nextA;
    }
}

void dump(uint8_t *mem, size_t len) {
    int i;
    for (i = 0; i < len; ++i)
        printf("%x ", mem[i]);
    printf("\n");
}

void do_tests(void) {
    uint8_t keymem[16] = {
        0x0, 0xd, 0x0, 0x0, 0xa, 0x5, 0x3, 0x6,
        0xc, 0x0, 0xf, 0x1, 0xd, 0x8, 0x5, 0x9,
    };
    dump(keymem, 16);
    fn_22b(keymem, 0);
    fn_22b(keymem, 0);
    fn_22b(keymem, 0);
    fn_22b(keymem, 0);
    dump(keymem, 16);

    inverse_22b(keymem, 0);
    inverse_22b(keymem, 0);
    inverse_22b(keymem, 0);
    inverse_22b(keymem, 0);
    dump(keymem, 16);

    uint8_t bootmem[16] = {
        0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
        0x0, 0x0, 0xb, 0x5, 0x3, 0xf, 0x3, 0xf,
    };
    fn_22b(bootmem, 0xa);
    fn_22b(bootmem, 0xa);
    dump(bootmem, 16);

    inverse_22b(bootmem, 0xa);
    inverse_22b(bootmem, 0xa);
    dump(bootmem, 16);
}

int main(int argc, char **argv) {
    uint8_t mem[16] = { 0, };
    char c;
    int i;

    if (argc != 7 && argc != 17) {
        do_tests();
        return 0;
    }

    for (i = 0; i < argc - 1; ++i) {
        c = tolower(argv[i+1][0]);
        if (c >= '0' && c <= '9') {
            mem[i] = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            mem[i] = c - 'a' + 0xa;
        } else {
            printf("%c is not a hex char\n", c);
            return 1;
        }
    }

    inverse_22b(mem, 0);
    inverse_22b(mem, 0);
    if (argc == 17) {
        inverse_22b(mem, 0);
        inverse_22b(mem, 0);
    }
    dump(mem, argc - 2);

    return 0;
}
