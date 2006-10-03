/* uuterm, Copyright (C) 2006 Rich Felker; licensed under GNU GPL v2 only */

#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <locale.h>
#include <sys/select.h>
#include <sys/time.h> /* for broken systems */

#include "uuterm.h"

int main(int argc, char *argv[])
{
	struct uuterm t = { };
	struct uudisp d = { };
	int tty, max;
	int i, l;
	unsigned char b[256];
	fd_set fds;
	struct timeval tv;
	void *buf;

	setlocale(LC_CTYPE, "");

	d.cell_w = 8;
	d.cell_h = 16;

	if (uudisp_open(&d) < 0)
		return 1;

	for (i=1; i<argc; i++)
		argv[i-1] = argv[i];
	argv[i-1] = NULL;

	if ((tty = uutty_open(argc > 1 ? argv : NULL, d.w, d.h)) < 0)
		return 1;

	signal(SIGHUP, SIG_IGN);

	if (!(buf = uuterm_buf_alloc(d.w, d.h))) {
		uudisp_close(&d);
		return 1;
	}
	uuterm_replace_buffer(&t, d.w, d.h, buf);
	uuterm_reset(&t);

	for (;;) {
		/* Setup fd_set containing fd's used by display and our tty */
		FD_ZERO(&fds);
		FD_SET(tty, &fds);
		max = uudisp_fd_set(&d, tty, &fds);
		tv.tv_sec = 0;
		tv.tv_usec = 250000;
		if (select(max, &fds, NULL, NULL, &tv) == 0)
			d.blink++;

		/* Process input from the tty, up to buffer size */
		if (FD_ISSET(tty, &fds)) {
			if ((l = read(tty, b, sizeof b)) <= 0)
				break;
			for (i=0; i<l; i++) {
				uuterm_stuff_byte(&t, b[i]);
				if (t.reslen) write(tty, t.res, t.reslen);
			}
		}

		/* Look for events from the display */
		uudisp_next_event(&d, &fds);

		if (d.w != t.w || d.h != t.h) {
			void *newbuf = uuterm_buf_alloc(d.w, d.h);
			if (newbuf) {
				uuterm_replace_buffer(&t, d.w, d.h, newbuf);
				uuterm_free(buf);
				buf = newbuf;
				uutty_resize(tty, d.w, d.h);
			}
		}
		if (d.inlen) {
			write(tty, d.intext, d.inlen);
			d.blink = 1;
		}

		/* If no more input is pending, refresh display */
		FD_ZERO(&fds);
		FD_SET(tty, &fds);
		tv.tv_sec = tv.tv_usec = 0;
		select(max, &fds, NULL, NULL, &tv);
		if (!FD_ISSET(tty, &fds))
			uudisp_refresh(&d, &t);
	}

	uudisp_close(&d);

	return 0;
}
