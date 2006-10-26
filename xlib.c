/* uuterm, Copyright (C) 2006 Rich Felker; licensed under GNU GPL v2 only */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>

#include "uuterm.h"
#include "ucf.h"

struct priv
{
	int fd;
	Display *display;
	int screen;
	Window window;
	XIM im;
	XIC ic;
	GC wingc, cugc, gc;
	Pixmap pixmap;
	int *slices_y;
	int curs_x, curs_y;
	int curs_on;
	unsigned long colors[16];
	unsigned char *bits;
	XImage image;
	int x1, x2, color;
	char *pastebuf;
};

static int resize_window(struct uudisp *d, int w, int h)
{
	struct priv *p = (void *)&d->priv;
	Pixmap pixmap;
	void *slice_buf;
	int i;

	w /= d->cell_w;
	h /= d->cell_h;

	if (w == d->w && h == d->h && p->pixmap && p->slices_y)
		return 0;

	slice_buf = uuterm_alloc(sizeof(int) * h);
	if (!slice_buf) return -1;
	if (p->slices_y) uuterm_free(p->slices_y);
	p->slices_y = slice_buf;
	if (w < d->w) d->w = w;
	if (h < d->h) d->h = h;

	for (i=0; i<h; i++) p->slices_y[i] = -1;

	pixmap = XCreatePixmap(p->display, p->window,
		w * d->cell_w, h * d->cell_h,
		DefaultDepth(p->display, p->screen));
	if (!pixmap) return -1;
	if (p->pixmap) XFreePixmap(p->display, p->pixmap);
	p->pixmap = pixmap;

	d->w = w;
	d->h = h;

	return 0;
}

int uudisp_open(struct uudisp *d)
{
	struct priv *p = (void *)&d->priv;
	XGCValues values;
	XVisualInfo vi;
	XSizeHints size_hints;
	XClassHint class_hint;
	XWMHints wm_hints;
	unsigned long fevent;
	char *fake_argv[2] = { "uuterm-x11", NULL };
	char *s;
	int px_w, px_h;
	struct ucf *f = d->font;
	int i;

	/* safety */
	if (sizeof(*p) > sizeof(d->priv)) return -1;

	if (!(p->display = XOpenDisplay(NULL)))
		return -1;

	px_w = 80*d->cell_w;
	px_h = 24*d->cell_h;

	p->fd = ConnectionNumber(p->display);
	p->screen = DefaultScreen(p->display);
	p->window = XCreateSimpleWindow(p->display,
		DefaultRootWindow(p->display),
		0, 0, px_w, px_h, 2,
		BlackPixel(p->display, p->screen),
		BlackPixel(p->display, p->screen));

	size_hints.width_inc = d->cell_w;
	size_hints.height_inc = d->cell_h;
	size_hints.min_width = d->cell_w*2;
	size_hints.min_height = d->cell_h*2;
	size_hints.flags = PMinSize|PResizeInc;
	class_hint.res_name = class_hint.res_class = "UUTerm";
	XmbSetWMProperties(p->display, p->window, "UUTerm", "UUTerm",
		fake_argv, 1, &size_hints, NULL, &class_hint);

	XMapWindow(p->display, p->window);

	p->im = XOpenIM(p->display, 0, 0, 0);
	if (!p->im) {
		XSetLocaleModifiers("@im=none");
		p->im = XOpenIM(p->display, 0, 0, 0);
	}
	if (p->im) p->ic = XCreateIC(p->im, XNInputStyle, XIMPreeditNothing|XIMStatusNothing, NULL);

	if (p->ic) XGetICValues(p->ic, XNFilterEvents, &fevent, NULL);
	else fevent = 0;
	XSelectInput(p->display, p->window,
		KeyPressMask|ButtonPressMask
		|ExposureMask|StructureNotifyMask|fevent);

	resize_window(d, px_w, px_h);

	p->wingc = XCreateGC(p->display, p->window, 0, &values);
	p->cugc = XCreateGC(p->display, p->window, 0, &values);
	p->gc = XCreateGC(p->display, p->pixmap, 0, &values);
	XSetFunction(p->display, p->cugc, GXxor);
	XSetForeground(p->display, p->cugc, WhitePixel(p->display, p->screen));

	memset(&p->image, 0, sizeof p->image);
	p->image.width = d->cell_w * 80;
	p->image.height = d->cell_h;
	p->image.format = XYBitmap;
	p->image.byte_order = ImageByteOrder(p->display);
	p->image.bitmap_bit_order = MSBFirst;
	p->image.bitmap_pad = 8;
	p->image.depth = 1;
	p->image.bytes_per_line = (p->image.width+7)>>3;
	p->image.data = p->bits = uuterm_alloc(p->image.bytes_per_line * d->cell_h);
	XInitImage(&p->image);

	for (i=0; i<16; i++) {
		XColor color;
		int R = 0x55 * (i&1);
		int G = 0x55 * (i&2) >> 1;
		int B = 0x55 * (i&4) >> 2;
		if (i&8) {
			R += R + 0x55;
			G += G + 0x55;
			B += B + 0x55;
		} else if (i == 7) R = G = B = 0xAA;
		color.red = R<<8;
		color.green = G<<8;
		color.blue = B<<8;
		XAllocColor(p->display, DefaultColormap(p->display, p->screen), &color);
		p->colors[i] = color.pixel;
	}

	return 0;
error:
	return -1;
}

int uudisp_fd_set(struct uudisp *d, int tty, void *fds)
{
	struct priv *p = (struct priv *)&d->priv;
	FD_SET(p->fd, (fd_set *)fds);
	return p->fd > tty ? p->fd+1 : tty+1;
}

struct
{
	KeySym ks;
	char s[7], l;
} keys[] = {
	{ XK_BackSpace, "\177",1 },
	{ XK_Home,      "\033[1~",4 },
	{ XK_Insert,    "\033[2~",4 },
	{ XK_Delete,    "\033[3~",4 },
	{ XK_End,       "\033[4~",4 },
	{ XK_Page_Up,   "\033[5~",4 },
	{ XK_Page_Down, "\033[6~",4 },
	{ XK_Up,        "\033[A",3 },
	{ XK_Down,      "\033[B",3 },
	{ XK_Right,     "\033[C",3 },
	{ XK_Left,      "\033[D",3 },
	{ XK_F1,        "\033[[A",4 },
	{ XK_F2,        "\033[[B",4 },
	{ XK_F3,        "\033[[C",4 },
	{ XK_F4,        "\033[[D",4 },
	{ XK_F5,        "\033[[E",4 },
	{ XK_F6,        "\033[17~",5 },
	{ XK_F7,        "\033[18~",5 },
	{ XK_F8,        "\033[19~",5 },
	{ XK_F9,        "\033[20~",5 },
	{ XK_F10,       "\033[21~",5 },
	{ XK_F11,       "\033[23~",5 },
	{ XK_F12,       "\033[24~",5 },
	{ 0, "" }
};

static Bool keyev_pred(Display *xd, XEvent *ev, XPointer d)
{
	return !((struct uudisp *)d)->inlen || ev->type != KeyPress;
}

void uudisp_next_event(struct uudisp *d, void *fds)
{
	struct priv *p = (struct priv *)&d->priv;
	unsigned char b;
	XEvent ev;
	KeySym ks;
	size_t r, l = sizeof(d->inbuf);
	unsigned char *s = d->inbuf;
	char tmp[32], mbtmp[sizeof(tmp)*MB_LEN_MAX];
	wchar_t wtmp[sizeof(tmp)];
	int status;
	int i, n;
	int y1, y2;
	Atom xa_utf8 = XInternAtom(p->display, "UTF8_STRING", False);
	Atom xa_sel = XInternAtom(p->display, "SELECTION", False);
	Atom type;
	int fmt;

	if (!d->inlen && p->pastebuf)
		XFree(p->pastebuf);

	if (!FD_ISSET(p->fd, (fd_set *)fds)) return;

	while (XCheckIfEvent(p->display, &ev, keyev_pred, (void *)d)) {
		if (XFilterEvent(&ev, 0)) continue;
		switch (ev.type) {
		case FocusIn:
			if (p->ic) XSetICFocus(p->ic);
			break;
		case FocusOut:
			if (p->ic) XUnsetICFocus(p->ic);
			break;
		case Expose:
			y1 = ev.xexpose.y / d->cell_h;
			y2 = y1 + ev.xexpose.height / d->cell_h + 1;
			for (i=0; i<d->h; i++)
				if ((unsigned)p->slices_y[i]-y1 <= y2-y1)
					p->slices_y[i] = -1;
			break;
		case ConfigureNotify:
			resize_window(d, ev.xconfigure.width, ev.xconfigure.height);
			break;
		case SelectionNotify:
			if (ev.xselection.property == None) {
				if (ev.xselection.target == xa_utf8)
					XConvertSelection(p->display,
						XA_PRIMARY, XA_STRING,
						xa_sel, p->window,
						ev.xselection.time);
				break;
			}
			if (XGetWindowProperty(p->display, p->window,
				xa_sel, 0, 65536, 1, xa_utf8, &type,
				&fmt, (void *)&l, (void *)&r, &s)
				!= Success || type != xa_utf8) break;
			d->intext = s;
			d->inlen = l;
			break;
		case ButtonPress:
			if (ev.xbutton.button == 2 && !d->inlen) {
				XConvertSelection(p->display, XA_PRIMARY,
					xa_utf8, xa_sel, p->window,
					ev.xbutton.time);
			}
			break;
		case KeyPress:
			d->intext = d->inbuf;
			if (p->ic) {
				r = XmbLookupString(p->ic, (void *)&ev, tmp, sizeof(tmp), &ks, &status);
				switch(status) {
				case XLookupKeySym:
					r = 0;
					break;
				case XLookupChars:
					ks = 0;
				case XLookupBoth:
					break;
				default:
					continue;
				}
			} else r = XLookupString((void *)&ev, tmp, sizeof(tmp), &ks, 0);
			if (r>=sizeof(tmp)) continue;
			tmp[r] = 0;
			if (ev.xkey.state & Mod1Mask) {
				*s++ = '\033';
				l--;
			}
			for (i=0; keys[i].ks && keys[i].ks != ks; i++);
			if (keys[i].ks) {
				if (keys[i].l > l) continue;
				memcpy(s, keys[i].s, keys[i].l);
				s += keys[i].l;
				l -= keys[i].l;
			} else if ((ev.xkey.state & ControlMask)
			 && ks == XK_minus && l) {
				*s++ = '_' & 0x1f;
				l--;
			} else if ((ev.xkey.state & ControlMask)
			 && (ks == XK_2 || ks == XK_space) && l) {
				*s++ = 0;
				l--;
			} else if (!r) {
				continue;
			} else if (p->ic) {
				if (r > l) continue;
				memcpy(s, tmp, r);
				s += r;
				l -= r;
			} else {
				/* Deal with Latin-1 crap.. */
				for (i=0; i<=r; i++) wtmp[i] = tmp[i];
				r = wcstombs(mbtmp, wtmp, sizeof mbtmp);
				if ((int)r > 0 && r <= l) {
					memcpy(s, mbtmp, r);
					s += r;
					l -= r;
				}
			}
			d->inlen = s - d->inbuf;
			break;
		}
	}
}

void uudisp_close(struct uudisp *d)
{
	struct priv *p = (struct priv *)&d->priv;
	XCloseDisplay(p->display);
}

static void commit_cells(struct uudisp *d, int idx)
{
	struct priv *p = (void *)&d->priv;
	if (p->x2 < p->x1) return;
	XPutImage(p->display, p->pixmap, p->gc, &p->image,
		0, 0, p->x1*d->cell_w, idx*d->cell_h,
		(p->x2-p->x1+1)*d->cell_w, d->cell_h);
}

void uudisp_predraw_cell(struct uudisp *d, int idx, int x, int color)
{
	struct priv *p = (void *)&d->priv;
	int i;
	unsigned char *dest;

	if (x != p->x2+1 || x-p->x1 >= 80 || color != p->color) {
		commit_cells(d, idx);
		p->x1 = x;
		p->color = color;
		XSetForeground(p->display, p->gc, p->colors[color&15]);
		XSetBackground(p->display, p->gc, p->colors[color>>4]);
	}

	p->x2 = x;
	dest = p->bits + x - p->x1;
	for (i=d->cell_h; i; i--, dest += p->image.bytes_per_line)
		*dest = 0;
}

void uudisp_draw_glyph(struct uudisp *d, int idx, int x, const void *glyph)
{
	struct priv *p = (void *)&d->priv;
	int i;
	const unsigned char *src = (void *)glyph;
	unsigned char *dest = p->bits + x - p->x1;

	for (i=d->cell_h; i; i--, dest += p->image.bytes_per_line)
		*dest |= *src++;
}

static void blit_slice(struct uudisp *d, int idx, int x1, int x2)
{
	struct priv *p = (void *)&d->priv;

	XCopyArea(p->display, p->pixmap, p->window, p->wingc,
		x1*d->cell_w, idx*d->cell_h,
		(x2-x1+1)*d->cell_w, d->cell_h,
		x1*d->cell_w, p->slices_y[idx]*d->cell_h);
}

void uudisp_refresh(struct uudisp *d, struct uuterm *t)
{
	struct priv *p = (void *)&d->priv;
	int h = t->h < d->h ? t->h : d->h;
	int x1, x2, idx, y;

	/* Clean up cursor first.. */
	if (p->curs_on && (!(d->blink&1) || t->x != p->curs_x || t->y != p->curs_y)) {
		idx = t->rows[p->curs_y]->idx;
		if ((unsigned)p->slices_y[idx] < d->h)
			blit_slice(d, idx, p->curs_x, p->curs_x);
		p->curs_on = 0;
	}

	for (y=0; y<h; y++) {
		x1 = t->rows[y]->x1;
		x2 = t->rows[y]->x2;
		idx = t->rows[y]->idx;
		if (x2 >= x1) {
			p->x1 = 0;
			p->x2 = -2;
			uuterm_refresh_row(d, t->rows[y], x1, x2);
			commit_cells(d, idx);
			t->rows[y]->x1 = t->w;
			t->rows[y]->x2 = -1;
		}
		if (p->slices_y[idx] != y) {
			p->slices_y[idx] = y;
			x1 = 0;
			x2 = d->w-1;
		} else if (x2 < x1) continue;
		blit_slice(d, idx, x1, x2);
	}
	p->curs_x = t->x;
	p->curs_y = t->y;
	if ((d->blink & 1) && !p->curs_on)
		XFillRectangle(p->display, p->window, p->cugc,
			p->curs_x*d->cell_w, p->curs_y*d->cell_h,
			d->cell_w, d->cell_h);
	p->curs_on = (d->blink & 1);
	XFlush(p->display);
}
