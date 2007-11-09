# uuterm, Copyright (C) 2006 Rich Felker; licensed under GNU GPL v2 only

default: all

ifeq ($(HOSTCC),)
HOSTCC = $(CC)
endif

SRCS = main.c term.c cell.c decomp.c tty.c alloc.c refresh.c ascii.c ucf.c font_load.c
OBJS = $(SRCS:.c=.o)
OBJS_FB = fbcon.o dblbuf.o
OBJS_X11 = xlib.o
LDFLAGS_X11 = -L/usr/X11R6/lib

YTTY_BASE = ./ytty/

ALL := uuterm-x11 ucfcomp $(YTTY_BASE)ytty.ucf
CLEAN := $(ALL) $(OBJS) $(OBJS_FB) $(OBJS_X11)

include $(YTTY_BASE)Makefile

CFLAGS = -O2 -s #-g
LDFLAGS = -s

-include config.mak

all: $(ALL)

$(OBJS) $(OBJS_FB) $(OBJS_X11): uuterm.h

decomp.o: decomp.h

uuterm-fb: $(OBJS) $(OBJS_FB)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(OBJS_FB) $(LIBS)

uuterm-x11: $(OBJS) $(OBJS_X11)
	$(CC) $(LDFLAGS) $(LDFLAGS_X11) -o $@ $(OBJS) $(OBJS_X11) -lX11 $(LIBS)

ucfcomp: ucfcomp.c
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $<

clean:
	rm -f $(CLEAN)
