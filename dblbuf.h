


struct slice
{
	int y;
	unsigned char *colors;
	unsigned char *bitmap;
};

struct dblbuf
{
	struct slice *slices;
	unsigned cs, ch;

	unsigned curs_x;
	unsigned curs_y;

	unsigned char *vidmem;
	unsigned row_stride;
	unsigned line_stride;
	unsigned bytes_per_pixel;
};

#define SLICE_BUF_SIZE(w, h, cs, ch) \
	( (h)*(sizeof(struct slice) + (w)*(1 + (cs)*(ch))) )

struct slice *dblbuf_setup_buf(int, int, int, int, unsigned char *);

