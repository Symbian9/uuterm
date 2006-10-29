# uuterm, Copyright (C) 2006 Rich Felker; licensed under GNU GPL v2 only


SRCS = main.c term.c cell.c decomp.c tty.c alloc.c refresh.c ascii.c ucf.c font_load.c

OBJS_FB = fbcon.o dblbuf.o
OBJS_X11 = xlib.o
LDFLAGS_X11 = -L/usr/X11R6/lib

ALL = uuterm-x11 uuterm-fb

CFLAGS = -O2 -s #-g
LDFLAGS = -s

-include config.mak

OBJS = $(SRCS:.c=.o)

all: $(ALL)

$(OBJS) $(OBJS_FB) $(OBJS_X11): uuterm.h

uuterm-fb: $(OBJS) $(OBJS_FB)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(OBJS_FB) $(LIBS)

uuterm-x11: $(OBJS) $(OBJS_X11)
	$(CC) $(LDFLAGS) $(LDFLAGS_X11) -o $@ $(OBJS) $(OBJS_X11) -lX11 $(LIBS)

clean:
	rm -f $(OBJS) $(OBJS_FB) $(OBJS_X11) uuterm-fb uuterm-x11

