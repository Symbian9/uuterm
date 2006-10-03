/* uuterm, Copyright (C) 2006 Rich Felker; licensed under GNU GPL v2 only */

#include "uuterm.h"
#include "dblbuf.h"

#if 0
static void blitline8_crap(unsigned char *dest, unsigned char *src, unsigned char *colors, int w, int cw)
{
	int cs = (cw+7)>>3;
	int skip = (-cw)&7;
	int x, i, j;
	signed char b;
	for (x=0; x<w; x++) {
		j=skip;
		b=*src++<<skip;
		for (i=0; ; i++) {
			for (; j<8; j++, b<<=1)
				//*dest++ = 7 & (b>>7);
				*dest++ = (*colors&15&(b>>7)) | (*colors>>4&~(b>>7));
			if (i >= cs) break;
			b=*src++;
			j=0;
		}
		colors++;
	}
}

static void blitline8_2(unsigned char *dest, unsigned char *src, unsigned char *colors, int w, int cw)
{
	int cs = (cw+7)>>3;
	int skip = (-cw)&7;
	int x, i, j;
	signed char b;
	unsigned char fg, bg;

	for (x=0; x<w; x++) {
		j=skip;
		b=*src++<<skip;
		fg = *colors & 15;
		bg = *colors++ >> 4;
		for (i=0; ; i++) {
			for (; j<8; j++, b<<=1)
				*dest++ = (fg&(b>>7)) | (bg&~(b>>7));
			if (i >= cs) break;
			b=*src++;
			j=0;
		}
	}
}
#endif

#ifdef HAVE_I386_ASM

#define BLIT_PIXEL_8 \
		"add %%al,%%al           \n\t" \
		"sbb %%dl,%%dl           \n\t" \
		"and %%bh,%%dl           \n\t" \
		"mov %%bl,%%dh           \n\t" \
		"add %%dl,%%dh           \n\t" \
		"mov %%dh,(%%edi)        \n\t" \
		"inc %%edi               \n\t" \

static void blitline8(unsigned char *dest, unsigned char *src, unsigned char *colors, int w, int cw)
{
	__asm__ __volatile__(
		"push %%ebp              \n\t"
		"mov %%ebx, %%ebp        \n\t"
		"\n1:                    \n\t"
		"mov (%%esi), %%al       \n\t"
		"inc %%esi               \n\t"
		"mov (%%ecx), %%bl       \n\t"
		"inc %%ecx               \n\t"
		"mov %%bl, %%bh          \n\t"
		"shr $4, %%bl            \n\t"
		"and $15, %%bh           \n\t"
		"sub %%bl, %%bh          \n\t"
		BLIT_PIXEL_8
		BLIT_PIXEL_8
		BLIT_PIXEL_8
		BLIT_PIXEL_8
		BLIT_PIXEL_8
		BLIT_PIXEL_8
		BLIT_PIXEL_8
		BLIT_PIXEL_8
		"dec %%ebp               \n\t"
		"jnz 1b                  \n\t"
		"pop %%ebp               \n\t"
		: "=S"(src), "=D"(dest), "=c"(colors), "=b"(w)
		: "S"(src), "D"(dest), "c"(colors), "b"(w)
		: "memory" );
}
#else
static void blitline8(unsigned char *dest, unsigned char *src, unsigned char *colors, int w, int cw)
{
	int x;
	unsigned char b;
	unsigned char c[2];

	for (x=0; x<w; x++) {
		b=*src++;
		c[1] = *colors & 15;
		c[0] = *colors++ >> 4;
		dest[0] = c[b>>7]; b<<=1;
		dest[1] = c[b>>7]; b<<=1;
		dest[2] = c[b>>7]; b<<=1;
		dest[3] = c[b>>7]; b<<=1;
		dest[4] = c[b>>7]; b<<=1;
		dest[5] = c[b>>7]; b<<=1;
		dest[6] = c[b>>7]; b<<=1;
		dest[7] = c[b>>7]; b<<=1;
		dest += 8;
	}
}
#endif

static void blit_slice(struct uudisp *d, int idx, int x1, int x2)
{
	struct dblbuf *b = (void *)&d->priv;
	int cs = (d->cell_w+7)>>3;
	int y = b->slices[idx].y;
	int w = x2 - x1 + 1;
	int s = d->w * cs;
	int i;

	unsigned char *dest = b->vidmem + y*b->row_stride
		+ x1*d->cell_w*b->bytes_per_pixel;
	unsigned char *src = b->slices[idx].bitmap + x1*cs;
	unsigned char *colors = b->slices[idx].colors + x1;

	for (i=0; i<d->cell_h; i++) {
		blitline8(dest, src, colors, w, d->cell_w);
		dest += b->line_stride;
		src += s;
	}
}

void clear_cells(struct uudisp *d, int idx, int x1, int x2)
{
	struct dblbuf *b = (void *)&d->priv;
	int i;
	int cs = d->cell_w+7 >> 3;
	int cnt = (x2 - x1 + 1) * cs;
	int stride = d->w * cs;
	unsigned char *dest = b->slices[idx].bitmap + x1 * cs;

	memset(b->slices[idx].colors + x1, 0, x2-x1+1);
	for (i=d->cell_h; i; i--, dest += stride)
		memset(dest, 0, cnt);
}

void uudisp_draw_glyph(struct uudisp *d, int idx, int x, const void *glyph, int color)
{
	struct dblbuf *b = (void *)&d->priv;
	int i;
	int cs = d->cell_w+7 >> 3;
	int stride = d->w * cs;
	unsigned char *src = (void *)glyph;
	unsigned char *dest = b->slices[idx].bitmap + cs * x;

	b->slices[idx].colors[x] = color;
	for (i=d->cell_h; i; i--, dest += stride)
		*dest |= *src++;
}

void uudisp_refresh(struct uudisp *d, struct uuterm *t)
{
	struct dblbuf *b = (void *)&d->priv;
	int h = t->h < d->h ? t->h : d->h;
	int y;

	/* Clean up cursor first.. */
	blit_slice(d, t->rows[b->curs_y]->idx, b->curs_x, b->curs_x);
	//printf("--- %d\r\n", b->slices[t->rows[b->curs_y]->idx].y);

	for (y=0; y<h; y++) {
		int idx = t->rows[y]->idx;
		int x1 = t->rows[y]->x1;
		int x2 = t->rows[y]->x2;
		if (x2 >= x1) {
			clear_cells(d, idx, x1, x2);
			uuterm_refresh_row(d, t->rows[y], x1, x2);
			t->rows[y]->x1 = t->w;
			t->rows[y]->x2 = -1;
		}
		if (b->slices[idx].y != y) {
			b->slices[idx].y = y;
			x1 = 0;
			x2 = d->w-1;
		} else if (x2 < x1) continue;
		blit_slice(d, idx, x1, x2);
	}

	if (d->blink & 1) {
		int idx = t->rows[t->y]->idx;
		b->slices[idx].colors[t->x] ^= 0xff;
		blit_slice(d, idx, t->x, t->x);
		b->slices[idx].colors[t->x] ^= 0xff;
	}
	b->curs_x = t->x;
	b->curs_y = t->y;
	//printf("+++ %d\r\n", b->slices[t->rows[b->curs_y]->idx].y);
}

struct slice *dblbuf_setup_buf(int w, int h, int cs, int ch, unsigned char *mem)
{
	struct slice *slices = (void *)mem;
	int i;

	mem += sizeof(slices[0]) * h;

	for (i=0; i<h; i++, mem += w) {
		slices[i].y = -1;
		slices[i].colors = mem;
	}
	w *= cs * ch;
	for (i=0; i<h; i++, mem += w)
		slices[i].bitmap = mem;

	return slices;
}
