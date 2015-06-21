#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef uint8_t u8;

static void hexdump(u8 *ptr, unsigned len, int bytes);

u8 mem[0x20] = { 0, };
int main(void) {
    memset(mem, 0xf, sizeof(mem) - 2);
    // hexdump(mem, sizeof(mem), 0);

    int i;
    u8 A = 5;
    int carry = 1;

    for (i = 0; i < sizeof(mem) - 2; ++i) {
        if (!(mem[i] & 1))
            A += 8;
        if (!(A & 2))
            A += 4;
        A = (A + mem[i]) & 0xf;
        mem[i] = A;

        if (!carry)
            A += 7;

        A = (A + mem[i]) & 0xF;
        A = A + mem[i] + carry;
        if (A >= 0x10) {
            carry = 1;
            A -= 0x10;
        } else {
            carry = 0;
        }
        mem[i] = (~A) & 0xf;
        A = mem[i];
    }

    hexdump(mem, sizeof(mem), 0);
    return 0;
}

static void hexdump(u8 *ptr, unsigned len, int bytes) {
    int i;

    for (i = 0; i < len; ++i) {
        if (bytes)
            printf("%02x ", ptr[i]);
        else
            printf("%x ", ptr[i] & 0xf);
        if ((i & 15) == 15)
            printf("\n");
    }
}
