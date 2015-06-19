PROG = sm5emu
OBJS = emu.o

CFLAGS=-g -Wall -Werror

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) -o $(PROG) $(OBJS)

clean:
	rm -f $(PROG) $(OBJS)
