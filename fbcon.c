/* uuterm, Copyright (C) 2006 Rich Felker; licensed under GNU GPL v2 only */

#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <signal.h>
#include <termios.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <sys/vt.h>
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
		p->b.repaint = 1;
		p->b.curs_x = p->b.curs_y = 0;
	}

	return 0;
}

static struct uudisp *display;

static void fatalsignal(int sig)
{
	uudisp_close(display);
	signal(sig, SIG_DFL);
	raise(sig);
}

static void dummy(int x)
{
}

static void vtswitch(int sig)
{
	struct priv *p = (void *)&display->priv;
	p->b.repaint = p->b.active = sig == SIGUSR2;
	ioctl(p->kb, VT_RELDISP, VT_ACKACQ);
	signal(sig, vtswitch);
}

int uudisp_open(struct uudisp *d)
{
	struct priv *p = (void *)&d->priv;
	struct fb_fix_screeninfo fix;
	struct termios tio;
	struct vt_mode vtm;

	p->fb = p->kb = p->ms = -1;
	p->b.vidmem = MAP_FAILED;

	if ((p->fb = open("/dev/fb0", O_RDWR)) < 0 || get_fb_size(d) < 0
	 || ioctl(p->fb, FBIOGET_FSCREENINFO, &fix) < 0)
		goto error;

	p->b.vidmem = mmap(0, fix.smem_len, PROT_WRITE|PROT_READ, MAP_SHARED, p->fb, 0);
	if (p->b.vidmem == MAP_FAILED)
		goto error;

	display = d;

	signal(SIGINT, fatalsignal);
	signal(SIGTERM, fatalsignal);
	signal(SIGSEGV, fatalsignal);
	signal(SIGBUS, fatalsignal);
	signal(SIGABRT, fatalsignal);
	signal(SIGFPE, fatalsignal);

	signal(SIGUSR1, vtswitch);
	signal(SIGUSR2, vtswitch);

	if ((p->kb = open("/dev/tty", O_RDONLY)) < 0
	 || ioctl(p->kb, KDSETMODE, KD_GRAPHICS) < 0)
		goto error;

	/* If the above succeeds, the below cannot fail */
	tcgetattr(p->kb, &p->tio);
	tio = p->tio;
	tio.c_cflag = B38400 | CS8 | CLOCAL | CREAD;
	tio.c_iflag = 0;
	tio.c_oflag = 0;
	tio.c_lflag = 0;
	tcsetattr(p->kb, TCSANOW, &tio);

	vtm.mode = VT_PROCESS;
	vtm.waitv = 0;
	vtm.relsig = SIGUSR1;
	vtm.acqsig = SIGUSR2;
	vtm.frsig = 0;
	ioctl(p->kb, VT_SETMODE, &vtm);

	signal(SIGWINCH, dummy); /* just to interrupt select! */
	signal(SIGTSTP, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);

	/* FIXME: need to actually detect if the VC is active */
	p->b.active = 1;

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
	if (!d->inlen) FD_SET(p->kb, (fd_set *)fds);
	return p->kb > tty ? p->kb+1 : tty+1;
}

void uudisp_next_event(struct uudisp *d, void *fds)
{
	struct priv *p = (struct priv *)&d->priv;
	unsigned char b;

	get_fb_size(d);

	if (FD_ISSET(p->kb, (fd_set *)fds)) {
		d->intext = d->inbuf;
		d->inlen = read(p->kb, d->inbuf, sizeof d->inbuf);
		if (d->inlen < 0) d->inlen = 0;
	}
}

void uudisp_close(struct uudisp *d)
{
	struct priv *p = (struct priv *)&d->priv;
	struct vt_mode vtm;
	tcsetattr(p->kb, TCSANOW, &p->tio);
	ioctl(p->kb, KDSETMODE, KD_TEXT);
	vtm.mode = VT_AUTO;
	vtm.waitv = 0;
	ioctl(p->kb, VT_SETMODE, &vtm);
	close(p->fb);
	close(p->kb);
	close(p->ms);
}
