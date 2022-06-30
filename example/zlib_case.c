#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <zlib.h>
#include "urn.h"
#include "3rd/base64.c"

void *zalloc(q, n, m)
	void *q;
	unsigned n, m;
{
	(void)q;
	return calloc(n, m);
}

void zfree(void *q, void *p)
{
	(void)q;
	free(p);
}

int inflate_raw(compr, comprLen, uncompr, uncomprLen)
	Byte *compr, *uncompr;
	uLong comprLen, uncomprLen;
{
	int rv;
	z_stream d_stream; /* decompression stream */

	d_stream.zalloc = zalloc;
	d_stream.zfree  = zfree;
	d_stream.opaque = (voidpf)0;

	d_stream.next_in  = compr;
	d_stream.avail_in = 0;
	d_stream.next_out = uncompr;

	// inflate with no header.
	URN_RET_ON_RV(inflateInit2(&d_stream, -15), "inflateInit2 error");

	while (d_stream.total_out < uncomprLen && d_stream.total_in < comprLen) {
		d_stream.avail_in = d_stream.avail_out = 1; /* force small buffers */
		rv = inflate(&d_stream, Z_NO_FLUSH);
		if (rv == Z_STREAM_END) break;
		URN_RET_ON_RV(rv, "inflate error");
	}

	URN_RET_ON_RV(inflateEnd(&d_stream), "inflateEnd error");
	return 0;
}

int main(int argc, char **argv) {
	if (argc < 1) {
		printf("No string to inflate\n");
		exit(EINVAL);
	}
	size_t slen = strlen(argv[1]);
	size_t rawlen = 0;
	printf("INPUT   : %s\n", argv[1]);
	unsigned char *raw = base64_decode((unsigned char*)argv[1], slen, &rawlen);
	printf("DECODE64: %p %lu\n", raw, rawlen);
	for (int i=0; i<rawlen; i++)
		printf("%hhu ", raw[i]);

	size_t inflated_sz = rawlen*10;
	unsigned char inflated[inflated_sz];
	int rv = inflate_raw(raw, rawlen, inflated, inflated_sz);
	printf("inflated %d size %lu\n%s\n", rv, inflated_sz, inflated);
	if (rv == Z_OK) {
		URN_LOG("Z_OK");
	} else if (rv == Z_MEM_ERROR) {
		URN_LOG("Z_MEM_ERROR");
	} else if (rv == Z_BUF_ERROR) {
		URN_LOG("Zlib not enough room in the output buffer");
	} else if (rv == Z_DATA_ERROR) {
		URN_LOG("Zlib data was corrupted or incomplete");
	} else {
		URN_LOG("Zlib unknown rv");
	}
	return 0;
}
