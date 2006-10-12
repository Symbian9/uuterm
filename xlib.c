/* uuterm, Copyright (C) 2006 Rich Felker; licensed under GNU GPL v2 only */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

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
	GC wingc, cugc, bggc, fggc, maskgc;
	Pixmap pixmap;
	Pixmap *glyph_cache;
	int *slices_y;
	int curs_x, curs_y;
	int curs_on;
	unsigned long colors[16];
};

#if 0
static int get_fb_size(struct uudisp *d)
{
	struct priv *p = (struct priv *)&d->priv;

	if (ioctl(p->fb, FBIOGET_FSCREENINFO, &fix) < 0
	 || ioctl(p->fb, FBIOGET_VSCREENINFO, &var) < 0
	 || fix.type != FB_TYPE_PACKED_PIXELS)
		return -1;

	p->b.bytes_per_pixel = (var.bits_per_pixel+7)>>3;
	p->b.line_stride = fix.line_length;
	p->b.row_stride = fix.line_length * d->cell_h;

	if (var.xres != p->xres || var.yres != p->yres) {
		int w = var.xres / d->cell_w;
		int h = var.yres / d->cell_h;
		void *buf = uuterm_alloc(SLICE_BUF_SIZE(w, h, (d->cell_w+7)/8, d->cell_h));
		if (!buf) return -1;
		if (p->buf_mem) uuterm_free(p->buf_mem);
		p->buf_mem = buf;
		p->b.slices = dblbuf_setup_buf(w, h, (d->cell_w+7)/8, d->cell_h, buf);
		p->xres = var.xres;
		p->yres = var.yres;
		d->w = w;
		d->h = h;
		p->b.repaint = 1;
		p->b.curs_x = p->b.curs_y = 0;
	}

	return 0;
}
#endif

int uudisp_open(struct uudisp *d)
{
	struct priv *p = (void *)&d->priv;
	XGCValues values;
	XVisualInfo vi;
	char *s;
	int px_w, px_h;
	struct ucf *f = d->font;
	const unsigned char *glyphs, *end;
	int nglyphs = f->nglyphs;
	GC gc;
	int npages;
	XImage *image;
	int i, j, k;

	if (!(p->display = XOpenDisplay(NULL)))
		return -1;

	d->w = 80;
	d->h = 24;
	p->slices_y = uuterm_alloc(d->h*sizeof(int));
	px_w = d->w*d->cell_w;
	px_h = d->h*d->cell_h;

	p->fd = ConnectionNumber(p->display);
	p->screen = DefaultScreen(p->display);
	p->window = XCreateSimpleWindow(p->display,
		DefaultRootWindow(p->display),
		0, 0, px_w, px_h, 1,
		BlackPixel(p->display, p->screen),
		BlackPixel(p->display, p->screen));
	XSelectInput(p->display, p->window, KeyPressMask|ExposureMask);
	XMapWindow(p->display, p->window);

	//XSetLocaleModifiers("@im=none");
	//p->im = XOpenIM(p->display, 0, 0, 0);
	//p->ic = XCreateIC(p->im, XNInputStyle, XIMPreeditNothing|XIMStatusNothing);

	p->pixmap = XCreatePixmap(p->display, p->window, px_w, px_h,
		DefaultDepth(p->display, p->screen));

	p->wingc = XCreateGC(p->display, p->window, 0, &values);
	p->cugc = XCreateGC(p->display, p->window, 0, &values);
	p->fggc = XCreateGC(p->display, p->pixmap, 0, &values);
	p->bggc = XCreateGC(p->display, p->pixmap, 0, &values);
	p->maskgc = XCreateGC(p->display, p->pixmap, 0, &values);
	XSetFunction(p->display, p->cugc, GXxor);
	XSetFunction(p->display, p->fggc, GXor);
	XSetFunction(p->display, p->bggc, GXcopy);
	XSetFunction(p->display, p->maskgc, GXandInverted);
	XSetForeground(p->display, p->cugc, WhitePixel(p->display, p->screen));
	XSetBackground(p->display, p->fggc, BlackPixel(p->display, p->screen));
	XSetForeground(p->display, p->maskgc, WhitePixel(p->display, p->screen));
	XSetBackground(p->display, p->maskgc, BlackPixel(p->display, p->screen));

	npages = (nglyphs+1023) / 1024; // allows up to 64 pixel cell height..
	p->glyph_cache = calloc(sizeof(p->glyph_cache[0]), npages);

	glyphs = f->glyphs;
	end = glyphs + nglyphs*f->S;

	for (i=0; i<npages; i++) {
		unsigned char data[1024*f->S], *g = data;

		if (BitmapBitOrder(p->display) == LSBFirst) {
			memset(data, 0, sizeof data);
			for (k=0; k<sizeof data && glyphs < end; k++, glyphs++)
				for (j=0; j<8; j++)
					data[k] |= ((*glyphs>>(7-j))&1)<<j;
		} else g = (void *)f->glyphs;

		p->glyph_cache[i] = XCreatePixmap(p->display,
			DefaultRootWindow(p->display),
			d->cell_w, d->cell_h * 1024, 1);

		gc = XCreateGC(p->display, p->glyph_cache[i], 0, &values);
		XSetForeground(p->display, gc, WhitePixel(p->display, p->screen));
		XSetBackground(p->display, gc, BlackPixel(p->display, p->screen));
		XMatchVisualInfo(p->display, p->screen, 1, StaticGray, &vi);

		image = XCreateImage(p->display,
			vi.visual, 1, XYBitmap, (-d->cell_w)&7, g,
			d->cell_w, d->cell_h * 1024, 8, 0);
		XPutImage(p->display, p->glyph_cache[i], gc, image,
			0, 0, 0, 0, d->cell_w, d->cell_h * 1024);

		//XDestroyImage(image);
		XFreeGC(p->display, gc);
	}

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
	int i, n;
	int y1, y2;

	d->inlen = 0;
	d->intext = d->inbuf;

	if (!FD_ISSET(p->fd, (fd_set *)fds)) return;

	while (XPending(p->display)) {
		XNextEvent(p->display, &ev);
		switch (ev.type) {
		case Expose:
			y1 = ev.xexpose.y / d->cell_h;
			y2 = y1 + ev.xexpose.height / d->cell_h + 1;
			for (i=0; i<d->h; i++)
				if ((unsigned)p->slices_y[i]-y1 <= y2-y1)
					p->slices_y[i] = -1;
			break;
		case KeyPress:
#if 0
			// r = XmbLookupString(p->ic, (void *)&ev, s, l, &ks, &status);
			switch(status) {
			case XLookupChars:
			case XLookupBoth:
				//
			}
#endif
			r = XLookupString((void *)&ev, tmp, sizeof(tmp), &ks, 0);
			if (r>=sizeof(tmp)) continue;
			if ((ev.xkey.state & Mod1Mask) && l) {
				*s++ = '\033';
				l--;
			}
			for (i=0; keys[i].ks && keys[i].ks != ks; i++);
			if (keys[i].ks) {
				if (keys[i].l > l) continue;
				memcpy(s, keys[i].s, keys[i].l);
				s += keys[i].l;
				l -= keys[i].l;
				continue;
			}
			if ((ev.xkey.state & ControlMask) && ks == XK_minus && l) {
				*s++ = '_' & 0x1f;
				l--;
				continue;
			}
			if ((ev.xkey.state & ControlMask) && (ks == XK_2 || ks == XK_space) && l) {
				*s++ = 0;
				l--;
				continue;
			}
			if (!r) continue;

			/* Deal with Latin-1 crap.. */
			for (i=0; i<=r; i++) wtmp[i] = tmp[i];
			r = wcstombs(mbtmp, wtmp, sizeof mbtmp);
			if ((int)r > 0 && r <= l) {
				memcpy(s, mbtmp, r);
				s += r;
				l -= r;
			}
			break;
		}
	}
	d->inlen = s - d->inbuf;
}

void uudisp_close(struct uudisp *d)
{
	struct priv *p = (struct priv *)&d->priv;
	XCloseDisplay(p->display);
}



void uudisp_draw_glyph(struct uudisp *d, int idx, int x, const void *glyph)
{
	struct priv *p = (void *)&d->priv;
	struct ucf *f = d->font;
	const unsigned char *foo = glyph;
	int g = (foo-f->glyphs)/f->S;
	int gp = g >> 10;
	g &= 1023;

	XCopyPlane(p->display, p->glyph_cache[gp], p->pixmap, p->maskgc,
		0, g*d->cell_h, d->cell_w, d->cell_h,
		x * d->cell_w, idx * d->cell_h, 1);
	XCopyPlane(p->display, p->glyph_cache[gp], p->pixmap, p->fggc,
		0, g*d->cell_h, d->cell_w, d->cell_h,
		x * d->cell_w, idx * d->cell_h, 1);
}

void uudisp_predraw_cell(struct uudisp *d, int idx, int x, int color)
{
	struct priv *p = (void *)&d->priv;
	XSetForeground(p->display, p->fggc, p->colors[color&15]);
	XSetForeground(p->display, p->bggc, p->colors[color>>4]);
	XFillRectangle(p->display, p->pixmap, p->bggc,
		x*d->cell_w, idx*d->cell_h, d->cell_w, d->cell_h);
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
			uuterm_refresh_row(d, t->rows[y], x1, x2);
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
