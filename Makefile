# uuterm, Copyright (C) 2006 Rich Felker; licensed under GNU GPL v2 only


SRCS = main.c term.c comb.c decomp.c tty.c alloc.c refresh.c ascii.c ucf.c font_load.c
OBJS = $(SRCS:.c=.o)

OBJS_FB = fbcon.o dblbuf.o
OBJS_X11 = xlib.o
LDFLAGS_X11 = -L/usr/X11R6/lib

ALL = uuterm-fb uuterm-x11

CFLAGS = -O2 -s #-g
LDFLAGS = -s
LIBS = -L/usr/X11R6/lib -lX11

-include config.mak

all: $(ALL)

$(OBJS) $(OBJS_FB) $(OBJS_X11): uuterm.h

uuterm-fb: $(OBJS) $(OBJS_FB)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(OBJS_FB)

uuterm-x11: $(OBJS) $(OBJS_X11)
	$(CC) $(LDFLAGS) $(LDFLAGS_X11) -o $@ $(OBJS) $(OBJS_X11) -lX11

clean:
	rm -f $(OBJS) uuterm-fb uuterm-x11

