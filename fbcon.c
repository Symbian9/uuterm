/* uuterm, Copyright (C) 2006 Rich Felker; licensed under GNU GPL v2 only */

#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <signal.h>
#include <termios.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <sys/kd.h>
#include <linux/fb.h>

#include "uuterm.h"
#include "dblbuf.h"

struct priv
{
	/* common to all double buffered fb targets */
	struct dblbuf b;
	/* fbcon-specific */
	int fb;
	int kb;
	int ms;
	struct termios tio;
	int kbmode;
	int mod;
	int xres, yres;
	void *buf_mem;
};

static int get_fb_size(struct uudisp *d)
{
	struct priv *p = (struct priv *)&d->priv;
	struct fb_fix_screeninfo fix;
	struct fb_var_screeninfo var;

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
	}

	return 0;
}

static void dummy(int x)
{
}

static int mapkey(unsigned *m, unsigned k, unsigned char *s)
{
#define LSHIFT "\200"
#define LCTRL  "\201"
#define LALT   "\202"
#define RSHIFT "\203"
#define RCTRL  "\204"
#define RALT   "\205"
#define CAPSLK "\206"
#define NUMLK  "\207"
#define SCRLK  "\210"
#define WTF    "\377"
	static const unsigned char keymap[] =
		"\0\033" "1234567890-=\177"
		"\t"   "qwertyuiop[]\r"
		LCTRL  "asdfghjkl;'`"
		LSHIFT "\\zxcvbnm,./" RSHIFT "*"
		LALT   " " CAPSLK
		"\301\302\303\304\305\306\307\310\311\312"
		NUMLK SCRLK;
	static const unsigned char keymap_sh[] =
		"\0\033" "!@#$%^&*()_+\177"
		"\t"   "QWERTYUIOP{}\r"
		LCTRL  "ASDFGHJKL:\"~"
		LSHIFT "|ZXCVBNM<>?" RSHIFT "*"
		LALT   " " CAPSLK
		"\301\302\303\304\305\306\307\310\311\312"
		NUMLK SCRLK;
	//71...
	static const unsigned char keypad[] = "789-456+1230.";
	//64..95 useless
	//96...
	static const unsigned char keypad2[] = "\r" RCTRL WTF "/" RALT WTF;
	//102...
	static const unsigned char cursblk[] = "1A5DC4B623";

	unsigned c;
	unsigned rel = k & 0x80;
	int i = 0;

	k &= 0x7f;
	if (*m & 4) s[i++] = '\033';
	if (k < sizeof(keymap)) {
		c = keymap[k];
		if (c-0200 < 6) {
			c &= 15;
			if (rel) *m &= ~(1<<c);
			else *m |= 1<<c;
			return 0;
		}
		if (rel || c > 0x80) return 0;
		if (*m & 9) c = keymap_sh[k];
		if (*m & 18) {
			if (keymap_sh[k] >= '@') c = keymap_sh[k] & 0x1f;
			else c &= 0x1f;
		}
		s[i++] = c;
		return i;
	}
	if (k-102 < sizeof(cursblk)) {
		if (rel) return 0;
		s[i++] = '\033';
		s[i++] = '[';
		c = cursblk[k-102];
		s[i++] = c;
		if (c < 'A') s[i++] = '~';
		return i;
	}
	return 0;
}

int uudisp_open(struct uudisp *d)
{
	struct priv *p = (void *)&d->priv;
	struct fb_fix_screeninfo fix;
	struct termios tio;

	p->fb = p->kb = p->ms = -1;
	p->b.vidmem = MAP_FAILED;

	if ((p->fb = open("/dev/fb0", O_RDWR)) < 0 || get_fb_size(d) < 0
	 || ioctl(p->fb, FBIOGET_FSCREENINFO, &fix) < 0)
		goto error;

	p->b.vidmem = mmap(0, fix.smem_len, PROT_WRITE|PROT_READ, MAP_SHARED, p->fb, 0);
	if (p->b.vidmem == MAP_FAILED)
		goto error;

	if ((p->kb = open("/dev/tty", O_RDONLY)) < 0
	 || ioctl(p->kb, KDGKBMODE, &p->kbmode) < 0
	 || ioctl(p->kb, KDSKBMODE, K_MEDIUMRAW) < 0)
		goto error;

	/* If the above succeeds, the below cannot fail */
	ioctl(p->kb, KDSETMODE, KD_GRAPHICS);
	tcgetattr(p->kb, &p->tio);
	tio = p->tio;
	tio.c_cflag = B38400 | CS8 | CLOCAL | CREAD;
	tio.c_iflag = 0;
	tio.c_oflag = 0;
	tio.c_lflag = 0;
	tcsetattr(p->kb, TCSANOW, &tio);

	signal(SIGWINCH, dummy); /* just to interrupt select! */
	signal(SIGTSTP, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);

	return 0;
error:
	if (p->b.vidmem != MAP_FAILED) munmap(p->b.vidmem, fix.smem_len);
	close(p->fb);
	close(p->kb);
	close(p->ms);
	return -1;
}

int uudisp_fd_set(struct uudisp *d, int tty, void *fds)
{
	struct priv *p = (struct priv *)&d->priv;
	if (p->ms >= 0) FD_SET(p->ms, (fd_set *)fds);
	FD_SET(p->kb, (fd_set *)fds);
	return p->kb > tty ? p->kb+1 : tty+1;
}

void uudisp_next_event(struct uudisp *d, void *fds)
{
	struct priv *p = (struct priv *)&d->priv;
	unsigned char b;

	get_fb_size(d);

	d->inlen = 0;
	d->intext = d->inbuf;

	if (FD_ISSET(p->kb, (fd_set *)fds) && read(p->kb, &b, 1) == 1)
		d->inlen = mapkey(&p->mod, b, d->intext);
}

void uudisp_close(struct uudisp *d)
{
	struct priv *p = (struct priv *)&d->priv;
	tcsetattr(p->kb, TCSANOW, &p->tio);
	ioctl(p->kb, KDSKBMODE, p->kbmode);
	ioctl(p->kb, KDSETMODE, KD_TEXT);
	close(p->fb);
	close(p->kb);
	close(p->ms);
}
