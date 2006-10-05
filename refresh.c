/* uuterm, Copyright (C) 2006 Rich Felker; licensed under GNU GPL v2 only */

#include "uuterm.h"
#include "ucf.h"

static void extract_cell(unsigned *ch, struct uucell *cell)
{
	int i;
	unsigned b;
	for (b=i=0; i<3; i++) b |= cell->c[i] << 8*i;
	ch[0] = b;
	ch -= 2;
	for (; i<sizeof(cell->c)+1 && cell->c[i]; i++)
		ch[i] = uu_combine_involution(b, cell->c[i]);
	if (cell->a & UU_ATTR_UL)
		ch[i++] = '_'; //0x0332;
	for (; i<sizeof(cell->c)+1; i++)
		ch[i] = 0;
}

const void *ascii_get_glyph(unsigned);

static const void *lookup_glyph(struct ucf *f, int i, unsigned *this, unsigned *prev, unsigned *next, int width, int part)
{
	const unsigned char *a;
	static const unsigned c[2] = { 0xfffd, 0 };
	int g = ucf_lookup(f, i, this, prev, next, width);
	if (g<0 && !i) g = ucf_lookup(f, i, c, c, c, width);
	if (g>=0) return f->glyphs + f->S * (g + part);
	return 0;
}

void uuterm_refresh_row(struct uudisp *d, struct uurow *row, int x1, int x2)
{
	unsigned ch[4][sizeof(row->cells[0].c)+1], *chp;
	int x, i;
	int width, part;

	if (x1) extract_cell(ch[(x1-1)&3], &row->cells[x1-1]);
	else memset(ch[3], 0, sizeof(ch[3]));
	extract_cell(ch[x1&3], &row->cells[x1]);

	for (x=x1; x<=x2; x++) {
		extract_cell(ch[(x+1)&3], &row->cells[x+1]);
		if (ch[x&3][0] == UU_FULLWIDTH) {
			width = 2; part = 0; chp = ch[(x+1)&3];
		} else if (ch[(x+3)&3][0] == UU_FULLWIDTH) {
			width = 2; part = 1; chp = ch[x&3];
		} else {
			width = 1; part = 0; chp = ch[x&3];
		}
		for (i=0; i<sizeof(ch[0]) && chp[i]; i++) {
			const void *glyph = lookup_glyph(d->font, i, chp, ch[(x+3)&3], ch[(x+1)&3], width, part);
			if (glyph) uudisp_draw_glyph(d, row->idx, x, glyph, row->cells[x].a & 0xff);
		}
	}
}


