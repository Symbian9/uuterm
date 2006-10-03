/* uuterm, Copyright (C) 2006 Rich Felker; licensed under GNU GPL v2 only */

#include <stdlib.h>
#include <sys/mman.h>

#include "uuterm.h"

void *uuterm_alloc(size_t len)
{
#ifdef MAP_ANONYMOUS
	size_t *mem = mmap(0, len+sizeof(size_t),
		PROT_READ|PROT_WRITE,
		MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if (mem == MAP_FAILED) return 0;
	*mem++ = len;
	return mem;
#else
	return malloc(len);
#endif
}

void uuterm_free(void *buf)
{
#ifdef MAP_ANONYMOUS
	size_t *mem = buf;
	mem--;
	munmap(mem, *mem);
#else
	free(buf);
#endif
}

void *uuterm_buf_alloc(int w, int h)
{
	/* FIXME: do we care about overflows? */
	return uuterm_alloc(UU_BUF_SIZE(w, h));
}
