#ifndef URANUS_ENCODING
#define URANUS_ENCODING

#include <zlib.h>

//////////////////////////////////////////
// Base64 encode/decode, see 3rd/base64.c
//////////////////////////////////////////

static const unsigned char base64_table[65] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const unsigned char base64_dtable[256] = {
	128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
	128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
	128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,  62, 128, 128, 128,  63,
	 52,  53,  54,  55,  56,  57,  58,  59,  60,  61, 128, 128, 128,   0, 128, 128,
	128,   0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,
         15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25, 128, 128, 128, 128, 128,
	128,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,
	 41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51, 128, 128, 128, 128, 128,
	128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
	128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
	128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
	128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
	128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
	128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
	128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
	128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128
};

int base64_decode_to(const unsigned char *src, unsigned char *out, size_t *out_len)
{
	unsigned char *pos, block[4], tmp;
	size_t i, count, olen;
	int pad = 0;

	const unsigned char *dtable = base64_dtable;

	count = 0;
	for (i = 0; src[i] != '\0'; i++)
		if (dtable[src[i]] != 0x80)
			count++;

	if (count == 0 || count % 4)
		return EINVAL;

	olen = count / 4 * 3;
	pos = out;

	count = 0;
	for (i = 0; src[i] != '\0'; i++) {
		tmp = dtable[src[i]];
		if (tmp == 0x80)
			continue;

		if (src[i] == '=')
			pad++;
		block[count] = tmp;
		count++;
		if (count == 4) {
			*pos++ = (block[0] << 2) | (block[1] >> 4);
			*pos++ = (block[1] << 4) | (block[2] >> 2);
			*pos++ = (block[2] << 6) | block[3];
			count = 0;
			if (pad) {
				if (pad == 1)
					pos--;
				else if (pad == 2)
					pos -= 2;
				else {
					/* Invalid padding */
					free(out);
					return EINVAL;
				}
				break;
			}
		}
	}

	*out_len = pos - out;
	return 0;
}

unsigned char * base64_decode(const unsigned char *src, size_t len,
			      size_t *out_len)
{
	unsigned char *out, *pos, block[4], tmp;
	size_t i, count, olen;
	int pad = 0;

	const unsigned char *dtable = base64_dtable;

	count = 0;
	for (i = 0; i < len; i++)
		if (dtable[src[i]] != 0x80)
			count++;

	if (count == 0 || count % 4)
		return NULL;

	olen = count / 4 * 3;
	pos = out = malloc(olen);
	if (out == NULL)
		return NULL;

	count = 0;
	for (i = 0; i < len; i++) {
		tmp = dtable[src[i]];
		if (tmp == 0x80)
			continue;

		if (src[i] == '=')
			pad++;
		block[count] = tmp;
		count++;
		if (count == 4) {
			*pos++ = (block[0] << 2) | (block[1] >> 4);
			*pos++ = (block[1] << 4) | (block[2] >> 2);
			*pos++ = (block[2] << 6) | block[3];
			count = 0;
			if (pad) {
				if (pad == 1)
					pos--;
				else if (pad == 2)
					pos -= 2;
				else {
					/* Invalid padding */
					free(out);
					return NULL;
				}
				break;
			}
		}
	}

	*out_len = pos - out;
	return out;
}

//////////////////////////////////////////
// Zlib
//////////////////////////////////////////

static void *zalloc(q, n, m)
	void *q;
	unsigned n, m;
{
	(void)q;
	return calloc(n, m);
}

static void zfree(void *q, void *p)
{
	(void)q;
	free(p);
}

int zlib_inflate_raw(unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t max_sz, size_t *sz)
{
	int rv;
	z_stream d_stream; /* decompression stream */

	d_stream.zalloc = zalloc;
	d_stream.zfree  = zfree;
	d_stream.opaque = (voidpf)0;

	d_stream.next_in  = compr;
	d_stream.avail_in = 0;
	d_stream.next_out = uncompr;

	// inflateInit2 windowSize-8..-15: no header.
	URN_RET_ON_RV(inflateInit2(&d_stream, -15), "zlib inflateInit2 error");

	while (d_stream.total_out < max_sz && d_stream.total_in < comprLen) {
		d_stream.avail_in = d_stream.avail_out = 1; /* force small buffers */
		rv = inflate(&d_stream, Z_NO_FLUSH);
		if (rv == Z_STREAM_END) break;
		URN_RET_ON_RV(rv, "zlib inflate error");
	}

	URN_RET_ON_RV(inflateEnd(&d_stream), "zlib inflateEnd error");
	*sz = d_stream.total_out;
	return 0;
}

#endif
