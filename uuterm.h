/* uuterm, Copyright (C) 2006 Rich Felker; licensed under GNU GPL v2 only */

#include <stddef.h>
#include <wchar.h> /* for mbstate_t... */

struct uucell
{
	unsigned short a;
	unsigned char c[8];
};

struct uurow
{
	int idx;
	int x1, x2; /* dirty region */
	struct uucell cells[1];
};

#define UU_ATTR_DIM     0x0100
#define UU_ATTR_UL      0x0200
#define UU_ATTR_REV     0x0400

#define UU_ATTR_BOLD    0x0008
#define UU_ATTR_FG      0x0007

#define UU_ATTR_BLINK   0x0080
#define UU_ATTR_BG      0x0070

struct uuterm
{
	struct uurow **rows;
	int w, h;

	// all members past this point are zero-filled on reset!
	int reset;

	// output state
	int x, y;
	int attr;
	int sr_y1, sr_y2;
	int ins  :1;
	int am   :1;
	int noam :1;

	// input state
	int ckp  :1;
	int kp   :1;
	char reslen;
	char res[16];

	// saved state
	int save_x, save_y;
	int save_attr, save_fg, save_bg;

	// escape sequence processing
	int esc;
	unsigned param[16];
	int nparam;

	// multibyte parsing
	mbstate_t mbs;
};

struct uudisp
{
	int cell_w, cell_h;
	int w, h;
	int inlen;
	unsigned char *intext;
	unsigned char inbuf[16];
	int blink;
	void *font;
	long priv[64];
};

#define UU_FULLWIDTH 0xfffe

#define UU_ROW_SIZE(w) (sizeof(struct uurow) + (w)*sizeof(struct uucell))
#define UU_BUF_SIZE(w, h) ( (h) * (sizeof(struct uurow *) * UU_ROW_SIZE((w))) )

void uuterm_reset(struct uuterm *);
void uuterm_replace_buffer(struct uuterm *, int, int, void *);
void uuterm_stuff_byte(struct uuterm *, unsigned char);

void uuterm_refresh_row(struct uudisp *, struct uurow *, int, int);

int uu_combine_involution(unsigned, unsigned);
int uu_decompose_char(unsigned, unsigned *, unsigned);

int uudisp_open(struct uudisp *);
int uudisp_fd_set(struct uudisp *, int, void *);
void uudisp_next_event(struct uudisp *, void *);
void uudisp_close(struct uudisp *);
void uudisp_refresh(struct uudisp *, struct uuterm *);
void uudisp_draw_glyph(struct uudisp *, int, int, const void *, int);

void *uuterm_alloc(size_t);
void uuterm_free(void *);
void *uuterm_buf_alloc(int, int);

void uutty_resize(int, int, int);
int uutty_open(char **, int, int);
