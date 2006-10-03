# uuterm, Copyright (C) 2006 Rich Felker; licensed under GNU GPL v2 only


SRCS = main.c term.c comb.c tty.c alloc.c refresh.c ascii.c     fbcon.c dblbuf.c
OBJS = $(SRCS:.c=.o)

CFLAGS = -O2 -s #-g
LDFLAGS = -s

-include config.mak

all: uuterm

$(OBJS): uuterm.h

uuterm: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

clean:
	rm -f $(OBJS) uuterm

