#include <err.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t u8;

void debugger(u8 op, u8 arg);
void decode(u8 op, u8 arg);

// ROM
u8 ROM[0x10][0x40];
#define FETCH(X) do { (X) = ROM[pc.page][pc.addr]; ++pc.addr; } while (0)

u8 RAM[0x40]; // A-series chips have 2x the RAM of non-A chips

typedef struct _pc_t {
    u8 page;
    u8 addr;
} pc_t;

pc_t pc = { 0, 0 };
pc_t frame_pc = { 0, 0 };
pc_t stack[4] = { { 0, }, };
unsigned sp = 0;

u8 A = 0, X = 0;
u8 BL = 0, BM = 0, SB = 0;
#define B ((BM << 4) | BL)
u8 C = 0;
int skip = 0;
int port[3] = { 0, 0, 0 };
int port2_hiz = 1;

// debugger control
int run = 0;
int verbose = 0;

int do_break = 0;
pc_t breakpoint = { 0, 0 };

int mem_break = 0;
u8 mem_break_addr = 0;
u8 mem_break_end = 0;
int hiz_break = 0;

// input data
unsigned cycle = 0;

int have_data = 0;
typedef struct _sample_t {
    unsigned ts;
    unsigned in;
} sample_t;
sample_t sample[100];
unsigned total_samples = 0;

static void hexdump(u8 *ptr, unsigned len, int bytes) {
    int i;

    for (i = 0; i < len; ++i) {
        if (bytes)
            printf("%02x ", ptr[i]);
        else
            printf("%x ", ptr[i]);
        if ((i & 15) == 15)
            printf("\n");
    }
}


////////////////////////////////
// instruction emulation
//


//////////////////
// address control

void op_TR(u8 op, u8 arg) {
    pc.addr = op & 0b111111;
}

void op_TL(u8 op, u8 arg) {
    pc.page = ((op & 0xf) << 2) | (arg >> 6);
    pc.addr = arg & 0b111111;
}

void op_TRS(u8 op, u8 arg) {
    if (sp == 4) {
        printf("overflow!\n");
        exit(1);
    }
    stack[sp] = pc;
    ++sp;
    pc.page = 0x1;
    pc.addr = (op & 0b11111) << 1;
}

void op_CALL(u8 op, u8 arg) {
    if (sp == 4) {
        printf("overflow!\n");
        exit(1);
    }
    stack[sp] = pc;
    ++sp;
    pc.page = ((op & 0xf) << 2) | (arg >> 6);
    pc.addr = arg & 0b111111;
}

void op_RTN(u8 op, u8 arg) {
    if (sp == 0) {
        printf("underflow!\n");
        exit(1);
    }
    --sp;
    pc = stack[sp];
}


////////////////
// data transfer

void op_LAX(u8 op, u8 arg) {
    A = op & 0b1111;
}

void op_LBMX(u8 op, u8 arg) {
    BM = op & 0b1111;
}

void op_LBLX(u8 op, u8 arg) {
    BL = op & 0b1111;
}

void op_LDA(u8 op, u8 arg) {
    A = RAM[B];
    BM ^= op & 0b11;
}

void op_EXC(u8 op, u8 arg) {
    u8 tmp = RAM[B];

    RAM[B] = A;
    A = tmp;
    BM ^= op & 0b11;
}

void op_EXCI(u8 op, u8 arg) {
    u8 tmp = RAM[B];

    RAM[B] = A;
    A = tmp;
    if (BL == 0x0F) {
        BL = 0;
        skip = 1;
    } else {
        ++BL;
    }
    BM ^= op & 0b11;
}

void op_EXCD(u8 op, u8 arg) {
    u8 tmp = RAM[B];

    RAM[B] = A;
    A = tmp;
    if (BL == 0) {
        BL = 0xF;
        skip = 1;
    } else {
        --BL;
    }
    BM ^= op & 0b11;
}

void op_EXAX(u8 op, u8 arg) {
    u8 tmp = X;
    X = A;
    A = tmp;
}

void op_ATX(u8 op, u8 arg) {
    X = A;
}

void op_EXBM(u8 op, u8 arg) {
    u8 tmp = A;
    A = BM;
    BM = tmp;
}

void op_EXBL(u8 op, u8 arg) {
    u8 tmp = A;
    A = BL;
    BL = tmp;
}

void op_EX(u8 op, u8 arg) {
    u8 tmp = SB;
    SB = B;
    BM = tmp >> 4;
    BL = tmp & 0xf;
}


/////////////
// arithmetic

void op_ADX(u8 op, u8 arg) {
    A = A + (op & 0b1111);
    if (A >= 0x10) {
        A %= 0x10;
        skip = 1;
    }
}

void op_ADD(u8 op, u8 arg) {
    A = (A + RAM[B]) % 0x10;
}

void op_ADC(u8 op, u8 arg) {
    A = A + RAM[B] + C;
    if (A >= 0x10) {
        A %= 0x10;
        C = 1;
        skip = 1;
    } else {
        C = 0;
    }
}

void op_COMA(u8 op, u8 arg) {
    A = (~A) & 0xf;
}

void op_INCB(u8 op, u8 arg) {
    ++BL;
    if (BL == 0x10) {
        BL = 0;
        skip = 1;
    }
}

void op_DECB(u8 op, u8 arg) {
    --BL;
    if (BL == 0xFF) {
        BL = 0xF;
        skip = 1; // FIXME test
    }
}


///////
// test

void op_TC(u8 op, u8 arg) {
    if (C)
        skip = 1;
}

void op_TM(u8 op, u8 arg) {
    if (RAM[B] & (1 << (op & 0b11)))
        skip = 1;
}

void op_TABL(u8 op, u8 arg) {
    if (A == BL)
        skip = 1;
}

void op_TPB(u8 op, u8 arg) {
    u8 num = op & 0b11;

    printf("%8u checking port %d [%d]\n", cycle, num, port[num]);

    if (num == 1) {
        if (have_data) {
            int i;
            for (i = 0; i < total_samples-1; ++i)
                if (sample[i+1].ts > cycle)
                    break;
            printf("using sample %d / %d\n", i+1, total_samples);
            port[1] = sample[i].in;
        } else { // flip bit on each call
            port[1] = 1 - port[1];
        }
    }

    if (num == 0) {
        skip = 1;
        return;
    }

    if (port[num])
        skip = 1;
}


///////////////////
// bit manipulation

void op_RM(u8 op, u8 arg) {
    u8 mask = 1 << (op & 0b11);
    RAM[B] &= ~mask;
}

void op_SC(u8 op, u8 arg) {
    C = 1;
}

void op_RC(u8 op, u8 arg) {
    C = 0;
}


/////////////
// IO control

void op_OUTL(u8 op, u8 arg) {
    printf("setting port0 to %x\n", A);
}

void op_OUT(u8 op, u8 arg) {
    if (BL == 0xf) {
        port2_hiz = A ? 0 : 1;
        printf("%8u port write hiz\n", cycle);
        printf("%8u port 2 write %x\n", cycle, port2_hiz ? 1 : port[0]);
    } else if (BL == 2) {
        port[0] = A;
        if (!port2_hiz)
            printf("%8u port 2 write %x\n", cycle, port[0]);
    }
}


/////////
// others

// load from ROM
void op_PAT(u8 op, u8 arg) {
    pc_t load;
    u8 romval;

    load.page = 4;
    load.addr = ((X & 0b11) << 4) | A;

    romval = ROM[load.page][load.addr];
    X = romval >> 4;
    A = romval & 0xf;
}

// read from secret ROM
void op_DTA(u8 op, u8 arg) {
    static u8 secret[8] = { 0xFC, 0xFC, 0xA5, 0x6C, 0x03, 0x8F, 0x1B, 0x9A };
    u8 offset, BL_t;

    if (BM >= 4 && BM <= 7) {
        offset = (BM - 4) * 2;
        if (BL < 8) {
            BL_t = BL;
        } else {
            BL_t = BL - 8;
            ++offset;
        }

        skip = (secret[offset] >> BL_t) & 1;
    }
}


typedef void (*op_handler_t)(u8 op, u8 arg);

void op_NOP(u8 op, u8 arg) {
    // do nuttin
}

void emulate(void) {
    u8 op = 0, arg = 0;
    op_handler_t handler = op_NOP;

    while (1) {
        frame_pc = pc;
        FETCH(op);
        // TODO check overflow

        // NOP
        if (op == 0x00) {
            handler = op_NOP;
        }

        // address control
        else if (op >= 0x80 && op <= 0xBF) {
            handler = op_TR;
        } else if (op >= 0xE0 && op <= 0xEF) {
            FETCH(arg);
            handler = op_TL;
        } else if (op >= 0xC0 && op <= 0xDF) {
            handler = op_TRS;
        } else if (op >= 0xF0 && op <= 0xFF) {
            FETCH(arg);
            handler = op_CALL;
        } else if (op == 0x7D) {
            handler = op_RTN;
        }

        // data transfer
        else if (op >= 0x10 && op <= 0x1F) {
            handler = op_LAX;
        } else if (op >= 0x30 && op <= 0x3F) {
            handler = op_LBMX;
        } else if (op >= 0x20 && op <= 0x2F) {
            handler = op_LBLX;
        } else if (op >= 0x50 && op <= 0x53) {
            handler = op_LDA;
        } else if (op >= 0x54 && op <= 0x57) {
            handler = op_EXC;
        } else if (op >= 0x58 && op <= 0x5B) {
            handler = op_EXCI;
        } else if (op >= 0x5C && op <= 0x5F) {
            handler = op_EXCD;
        } else if (op == 0x64) {
            handler = op_EXAX;
        } else if (op == 0x65) {
            handler = op_ATX;
        } else if (op == 0x66) {
            handler = op_EXBM;
        } else if (op == 0x67) {
            handler = op_EXBL;
        } else if (op == 0x68) {
            handler = op_EX;
        }

        // arithmetic
        else if (op >= 0x00 && op <= 0x0F) {
            handler = op_ADX;
        } else if (op == 0x7A) {
            handler = op_ADD;
        } else if (op == 0x7B) {
            handler = op_ADC;
        } else if (op == 0x79) {
            handler = op_COMA;
        } else if (op == 0x78) {
            handler = op_INCB;
        } else if (op == 0x7C) {
            handler = op_DECB;
        }

        // test
        else if (op == 0x6E) {
            handler = op_TC;
        } else if (op >= 0x48 && op <= 0x4B) {
            handler = op_TM;
        } else if (op == 0x6B) {
            handler = op_TABL;
        } else if (op >= 0x4C && op <= 0x4F) {
            handler = op_TPB;
        }

        // bit manip
        else if (op >= 0x40 && op <= 0x43) {
            handler = op_RM;
        }
        else if (op == 0x61) {
            handler = op_SC;
        } else if (op == 0x60) {
            handler = op_RC;
        }

        // io control
        else if (op == 0x71) {
            handler = op_OUTL;
        } else if (op == 0x75) {
            handler = op_OUT;
        }


        // unknown
        else if (op == 0x6A) {
            FETCH(arg);
            handler = op_PAT;
        } else if (op == 0x69) {
            FETCH(arg);
            handler = op_DTA;
        }


        else {
            printf("nope %02x\n", op);
            abort();
        }

        debugger(op, arg);

        cycle += pc.addr - frame_pc.addr;

        if (!skip) {
            handler(op, arg);
        } else {
            skip = 0;
        }
    }
}


////////////////////////////////
// debugger
//



void debugger(u8 op, u8 arg) {
    char buf[4096];
    char *tokens[16], *token;
    int i = 0, num = 0;

    while (1) {
        if (!skip && ((frame_pc.page == 1 && frame_pc.addr == 0x02)
            || (frame_pc.page == 0 && frame_pc.addr == 0x0a))) {
            run = 0;
            printf("Dead\n");
        }

        // breakpoint
        if (do_break && frame_pc.page == breakpoint.page && frame_pc.addr == breakpoint.addr) {
            run = 0;
            printf("Breakpoint\n");
        }

        if (mem_break &&
            ((op >= 0x50 && op <= 0x5F)
             || op == 0x7A || op == 0x7B
             || (op >= 0x48 && op <= 0x4B)
             || (op >= 0x40 && op <= 0x43))
            && B >= mem_break_addr && B <= mem_break_end) {

            run = 0;
            printf("Mem breakpoint\n");
        }

        // break on Hi-Z
        if (hiz_break && op == 0x75 && BL == 0xF) {
            run = 0;
        }


        if (run && !verbose)
            break;

        printf("%x.%02x : ", frame_pc.page, frame_pc.addr);
        decode(op, arg);

        printf("  PC=%x.%02x A=%x X=%x BM=%x BL=%x SB=%02x C=%d SP=%d skip=%d\n",
                frame_pc.page, frame_pc.addr, A, X, BM, BL, SB, C, sp, skip);
        printf("  P0=%x P1=%x P2=%x hiz=%d   cycle=%u div=%u\n", port[0], port[1], port[2], port2_hiz, cycle, (cycle / 2) & 0x7fff);

        if (run)
            break;

        printf("> ");

        // tokenize
        fgets(buf, 4095, stdin);
        buf[4095] = 0;
        for (i = 0; buf[i]; ++i)
            if (buf[i] == '\n') {
                buf[i] = 0;
                break;
            }

        i = 0;
        token = strtok(buf, " ");
        while (token) {
            tokens[i++] = token;
            token = strtok(NULL, " ");
        }
        num = i;

        // operate
        if (num == 0) {
            break;
        }

        if (strcmp(tokens[0], "p") == 0) {
            ; // do nothing: loop
        } else if (strcmp(tokens[0], "port") == 0) {
            if (num < 3) {
                printf("Error: port requires two args\n");
            } else {
                unsigned portnum, val;
                portnum = strtoul(tokens[1], NULL, 10);
                val = strtoul(tokens[2], NULL, 16);
                if (portnum > 2)
                    printf("Error: port must be between 0 and 2\n");
                else if (val > 0xf)
                    printf("Error: value must be between 0 and f\n");
                else
                    port[portnum] = val;
            }
        } else if (strcmp(tokens[0], "q") == 0) {
            exit(0);
        } else if (strcmp(tokens[0], "m") == 0) {
            hexdump(RAM, 0x40, 0);
        } else if (strcmp(tokens[0], "r") == 0) {
            run = 1;
            break;
        } else if (strcmp(tokens[0], "b") == 0) {
            if (num < 3) {
                printf("Error: b requires two args\n");
            } else {
                breakpoint.page = strtoul(tokens[1], NULL, 16);
                breakpoint.addr = strtoul(tokens[2], NULL, 16);
                do_break = 1;
                printf("breakpoint set at %x.%02x\n", breakpoint.page, breakpoint.addr);
            }
        } else if (strcmp(tokens[0], "cb") == 0) {
            do_break = 0;
        } else if (strcmp(tokens[0], "sp") == 0) {
            for (i = 0; i < sp; ++i)
                printf("  SP[%d] %x.%02x\n", i, stack[i].page, stack[i].addr);
        } else if (strcmp(tokens[0], "mb") == 0) {
            if (num < 2) {
                printf("Error: mb requires one or two args\n");
            } else {
                mem_break_addr = strtoul(tokens[1], NULL, 16);
                mem_break = 1;
                if (num == 2) {
                    mem_break_end = mem_break_addr;
                } else {
                    mem_break_end = strtoul(tokens[2], NULL, 16);
                }
                printf("memory breakpoint set on [%02x, %02x]\n", mem_break_addr, mem_break_end);
            }
        } else if (strcmp(tokens[0], "v") == 0) {
            verbose = 1;
        } else if (strcmp(tokens[0], "skip") == 0) {
            skip = 1 - skip;
        } else if (strcmp(tokens[0], "hiz") == 0) {
            hiz_break = 1;
        } else if (strcmp(tokens[0], "poke") == 0) {
            if (num < 3) {
                printf("Error: poke requires two args\n");
            } else {
                RAM[strtoul(tokens[1], NULL, 16)] = strtoul(tokens[2], NULL, 16);
            }
        }
    }
}

void decode(u8 op, u8 arg) {
    // NOP
    if (op == 0x00) {
        printf("nop\n");
    }

    // control
    else if (op >= 0x80 && op <= 0xBF) {
        printf("tr %02x\n", op & 0b111111);
    } else if (op >= 0xE0 && op <= 0xEF) {
        printf("tl %x.%02x\n", ((op & 0xf) << 2) | (arg >> 6), arg & 0b111111);
    } else if (op >= 0xC0 && op <= 0xDF) {
        printf("trs %x\n", op & 0b11111);
    } else if (op >= 0xF0 && op <= 0xFF) {
        printf("call %x.%02x\n", ((op & 0xf) << 2) | (arg >> 6), arg & 0b111111);
    } else if (op == 0x7D) {
        printf("rtn\n");
    }

    // data transfer
    else if (op >= 0x10 && op <= 0x1F) {
        printf("lax %x\n", op & 0b1111);
    } else if (op >= 0x30 && op <= 0x3F) {
        printf("lbmx %x\n", op & 0b1111);
    } else if (op >= 0x20 && op <= 0x2F) {
        printf("lblx %x\n", op & 0b1111);
    } else if (op >= 0x50 && op <= 0x53) {
        printf("lda %x\n", op & 0b11);
    } else if (op >= 0x54 && op <= 0x57) {
        printf("exc %x\n", op & 0b11);
    } else if (op >= 0x58 && op <= 0x5B) {
        printf("exci %x\n", op & 0b11);
    } else if (op >= 0x5C && op <= 0x5F) {
        printf("excd %d\n", op & 0b11);
    } else if (op == 0x64) {
        printf("exax\n");
    } else if (op == 0x65) {
        printf("atx\n");
    } else if (op == 0x66) {
        printf("exbm\n");
    } else if (op == 0x67) {
        printf("exbl\n");
    } else if (op == 0x68) {
        printf("ex\n");
    }

    // arithmetic
    else if (op >= 0x00 && op <= 0x0F) {
        printf("adx %x\n", op & 0b1111);
    } else if (op == 0x7A) {
        printf("add\n");
    } else if (op == 0x7B) {
        printf("adc\n");
    } else if (op == 0x79) {
        printf("coma\n");
    } else if (op == 0x78) {
        printf("incb\n");
    } else if (op == 0x7C) {
        printf("decb\n");
    }

    // test
    else if (op == 0x6E) {
        printf("tc\n");
    } else if (op >= 0x48 && op <= 0x4B) {
        printf("tm %x\n", op & 0b11);
    } else if (op == 0x6B) {
        printf("tabl\n");
    } else if (op >= 0x4C && op <= 0x4F) {
        printf("tpb %x\n", op & 0b11);
    }

    // bit manip
    else if (op >= 0x40 && op <= 0x43) {
        printf("rm %x\n", op & 0b11);
    } else if (op == 0x61) {
        printf("sc\n");
    } else if (op == 0x60) {
        printf("rc\n");
    }

    // io control
    else if (op == 0x71) {
        printf("outl\n");
    } else if (op == 0x75) {
        printf("out\n");
    }

    // unknown
    else if (op == 0x6A) {
        printf("pat %x\n", arg);
    } else if (op == 0x69) {
        printf("dta\n");
    }


    else {
        printf("unknown\n");
    }
}

void load_data(char *name) {
    FILE *file;
    unsigned ts, cic_in, foo, foo2;

    file = fopen(name, "r");
    if (file == NULL)
        err(1, "Can't open %s", name);

    while (fscanf(file, "%u,%u,%u,%u", &ts, &cic_in, &foo, &foo2) == 4) {
        sample[total_samples].ts = (ts - 10240625) / 1250; // nanoseconds -> cycles
        sample[total_samples].in = cic_in;
        ++total_samples;
    }

    fclose(file);
}

void stop_run(int signum) {
    run = 0;
}

int main(int argc, char **argv) {
    FILE *rom_file = NULL;
    int i;
    size_t r;

    if (argc < 2) {
        printf("Usage: %s <rom.bin> [<data.csv>]\n", argv[0]);
        return 1;
    }

    rom_file = fopen(argv[1], "r");
    if (rom_file == NULL) {
        err(1, "Can't open ROM");
    }

    for (i = 0; i < 0x10; ++i) {
        r = fread(ROM[i], 1, 0x40, rom_file);
        if (r != 0x40)
            warnx("File too short");
        if (r == 0)
            break;
    }
    fclose(rom_file);

    if (argc > 2) {
        have_data = 1;
        load_data(argv[2]);
    }

    signal(SIGINT, stop_run);
    srand(0);

    emulate();

    return 0;
}
