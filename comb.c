/* uuterm, Copyright (C) 2006 Rich Felker; licensed under GNU GPL v2 only */

#define R(a,b) { (a), (b)-(a) }

static const unsigned short common[][2] = {
	R( 0x300, 0x341 ),
	R( 0x346, 0x362 ),
	R( 0x200B, 0x200F ),
	R( 0x202A, 0x202E ),
	R( 0x2060, 0x206F ),
	R( 0x20D0, 0x20EA ),
	{ 0, 0 }
};

static const unsigned short latin[][2] = {
	R( 0x363, 0x36F ),
	{ 0, 0 }
};

static const unsigned short greek[][2] = {
	R( 0x342, 0x345 ),
	{ 0, 0 }
};

static const unsigned short cyrillic[][2] = {
	R( 0x483, 0x489 ),
	{ 0, 0 }
};

static const unsigned short hebrew[][2] = {
	R( 0x591, 0x5C4 ),
	{ 0, 0 }
};

static const unsigned short arabic[][2] = {
	R( 0x600, 0x603 ),
	R( 0x610, 0x615 ),
	R( 0x64B, 0x658 ),
	R( 0x670, 0x670 ),
	R( 0x6D6, 0x6ED ),
	{ 0, 0 }
};

static const unsigned short syriac[][2] = {
	R( 0x70F, 0x711 ),
	R( 0x730, 0x74A ),
	{ 0, 0 }
};

static const unsigned short thaana[][2] = {
	R( 0x7A6, 0x7B0 ),
	{ 0, 0 }
};

static const unsigned short devanagari[][2] = {
	R( 0x901, 0x902 ),
	R( 0x93C, 0x963 ),
	{ 0, 0 }
};

static const unsigned short bengali[][2] = {
	R( 0x981, 0x981 ),
	R( 0x9BC, 0x9E3 ),
	{ 0, 0 }
};

static const unsigned short gurmukhi[][2] = {
	R( 0xA01, 0xA02 ),
	R( 0xA3C, 0xA4D ),
	R( 0xA70, 0xA71 ),
	{ 0, 0 }
};

static const unsigned short gujarati[][2] = {
	R( 0xA81, 0xA82 ),
	R( 0xABC, 0xAE3 ),
	{ 0, 0 }
};

static const unsigned short oriya[][2] = {
	R( 0xB01, 0xB01 ),
	R( 0xB3C, 0xB4D ),
	R( 0xB56, 0xB56 ),
	{ 0, 0 }
};

static const unsigned short tamil[][2] = {
	R( 0xB82, 0xB82 ),
	R( 0xBC0, 0xBCD ),
	{ 0, 0 }
};

static const unsigned short telugu[][2] = {
	R( 0xC3E, 0xC56 ),
	{ 0, 0 }
};

static const unsigned short kannada[][2] = {
	R( 0xCBC, 0xCCD ),
	{ 0, 0 }
};

static const unsigned short malayalam[][2] = {
	R( 0xD41, 0xD4D ),
	{ 0, 0 }
};

static const unsigned short sinhala[][2] = {
	R( 0xDCA, 0xDD6 ),
	{ 0, 0 }
};

static const unsigned short thai[][2] = {
	R( 0xE31, 0xE3A ),
	R( 0xE47, 0xE4E ),
	{ 0, 0 }
};

static const unsigned short lao[][2] = {
	R( 0xEB1, 0xECD ),
	{ 0, 0 }
};

static const unsigned short tibetan[][2] = {
	R( 0xF18, 0xF19 ),
	R( 0xF35, 0xF35 ),
	R( 0xF39, 0xF39 ),
	R( 0xF71, 0xF84 ),
	R( 0xF90, 0xFBC ),
	R( 0xFC6, 0xFC6 ),
	{ 0, 0 }
};

static const unsigned short burmese[][2] = {
	R( 0x102D, 0x1039 ),
	R( 0x1058, 0x1059 ),
	{ 0, 0 }
};

static const unsigned short misc_scripts[][2] = {
	R( 0x1732, 0x1734 ), /* hanunoo */
	R( 0x1752, 0x1753 ), /* buhid */
	R( 0x17B4, 0x17BD ), /* khmer */
	R( 0x17C6, 0x17D3 ),
	R( 0x17DD, 0x17DD ),
	R( 0x18A9, 0x18A9 ), /* mongolian */
	R( 0x1920, 0x193B ), /* limbu (can be broken down more) */
	{ 0, 0 }
};

#undef R
#define R(a,b,s) { (a), (b)-(a), (s) }

static const struct {
	unsigned a, l;
	const unsigned short (*r)[2];
} scripts[] = {
	R( 0x400, 0x52F, cyrillic ),
	R( 0x590, 0x5FF, hebrew ),
	R( 0x600, 0x6FF, arabic ),
	R( 0x700, 0x74F, syriac ),
	R( 0x780, 0x7B1, thaana ),
	R( 0x900, 0x97F, devanagari ),
	R( 0x980, 0x9FF, bengali ),
	R( 0xA00, 0xA7F, gurmukhi ),
	R( 0xA80, 0xAFF, gujarati ),
	R( 0xB00, 0xB7F, oriya ),
	R( 0xB80, 0xBFF, tamil ),
	R( 0xC00, 0xC7F, telugu ),
	R( 0xC80, 0xCFF, kannada ),
	R( 0xD00, 0xD7F, malayalam ),
	R( 0xD80, 0xDFF, sinhala ),
	R( 0xF00, 0xFFF, tibetan ),
	R( 0x1000, 0x108F, burmese ),
	R( 0x1720, 0x19FF, misc_scripts ),
	R( 0x1D2B, 0x1D2B, cyrillic ),
	R( 0x0000, 0x10FFFF, common ),
	{ }
};

#undef R

int uu_combine_involution(unsigned b, unsigned c)
{
	int i;
	unsigned code = 1;
	const unsigned short (*r)[2];
	for (i=0; scripts[i].l; i++)
		if (b - scripts[i].a <= scripts[i].l)
			for (r = scripts[i].r; r[0][0]; code += r++[0][1])
				if (c - r[0][0] <= r[0][1])
					return c - r[0][0] + code;
				else if (c - code <= r[0][1])
					return c + r[0][0] - code;
	return 0;
}
