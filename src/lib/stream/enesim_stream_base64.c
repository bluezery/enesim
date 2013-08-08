/* ENESIM - Direct Rendering Library
 * Copyright (C) 2007-2011 Jorge Luis Zapata
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.
 * If not, see <http://www.gnu.org/licenses/>.
 */
#include "enesim_private.h"

#include "enesim_main.h"
#include "enesim_stream.h"
#include "enesim_stream_private.h"
#include <ctype.h>
/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/
typedef struct _Enesim_Stream_Base64
{
	/* passed in */
	Enesim_Stream *data;
	/* last decoded data */
	unsigned char last[3];
	/* the next value to read */
	int last_offset;
	size_t enclen;
	size_t declen;
	char *buf;
	char *curr;
	char *end;
	ssize_t offset;
} Enesim_Stream_Base64;

static Eina_Bool _base64_decode_digit(unsigned char c, unsigned char *v)
{
	switch (c)
	{
		case '+': *v = 62;
		break;

		case '/': *v = 63;
		break;

		case '=': *v = 0;
		break;

		default:
		if (isdigit(c)) *v = c - '0' + 52;
		else if (islower(c)) *v = c - 'a' + 26;
		else if (isupper(c)) *v = c - 'A';
		else return EINA_FALSE;
		break;
	}
	return EINA_TRUE;
}

static void _base64_decode(unsigned char *in, unsigned char *out)
{
	out[0] = (unsigned char)(in[0] << 2 | in[1] >> 4);
	out[1] = (unsigned char)(in[1] << 4 | in[2] >> 2);
	out[2] = (unsigned char)(((in[2] << 6) & 0xc0) | in[3]);
}

/* 4 bytes in base64 give 3 decoded bytes
 * so the input must be multiple of 4
 * and the output multiple of 3
 */
static void _base64_decode_stream(unsigned char *in, unsigned char *out, size_t len)
{
	size_t l = 0;

	while (l < len)
	{
		unsigned char dec[4];
		int i;

		/* read the four input bytes */
		for (i = 0; i < 4; i++)
		{
			if (!_base64_decode_digit(in[i], &dec[i]))
				printf("error %c\n", in[i]);
#if 0
			printf("read %c %d\n", in[i], dec[i]);
#endif
		}

		_base64_decode(dec, out);
		/* increment the source */
		in += 4;
		l += 4;
		/* increment the buffer */
		out += 3;
	}
}
/*----------------------------------------------------------------------------*
 *                      The Enesim Image Data interface                       *
 *----------------------------------------------------------------------------*/
/* when the user requests 3 bytes of base64 decoded data we need to read 4 bytes
 * so we always read more from the real source
 */
static ssize_t _enesim_stream_base64_read(void *data, void *buffer, size_t len)
{
	Enesim_Stream_Base64 *thiz = data;
	size_t extra = 0;
	int enclen;
	int declen;
	int rest;
	unsigned char *b = buffer;

	/* first read the missing bytes from the previous read */
	if (thiz->last_offset)
	{
		int offset;
		size_t i;

		offset = thiz->last_offset;
		extra = 3 - offset;
		if (len < extra)
		{
			extra = len;
		}
		for (i = offset; i < offset + extra; i++)
		{
			*b++ = thiz->last[i];
			len--;
		}
		thiz->last_offset += extra;
	}
	/* how many encoded blocks we need to read */
 	enclen = ((len + 2) / 3) * 4;
	if (thiz->curr + enclen > thiz->end)
	{
		/* if we pass the inner data, we need to adapt
		 * to the new sizes
		 */
		enclen = thiz->end - thiz->curr;
		len = (enclen / 4) * 3;
	}
	declen = len;
	rest = (declen % 3);
	/* check if we need to not read complete, if so
	 * only read all the data that fits on the buffer
	 * padded to 3 bytes
	 */
	if (rest)
	{
		declen -= rest;
		enclen -= 4;
	}
	_base64_decode_stream((unsigned char *)thiz->curr, b, enclen);
	b += declen;
	thiz->curr += enclen;

	if (rest)
	{
		int i;

		_base64_decode_stream((unsigned char *)thiz->curr, thiz->last, 4);
		for (i = 0; i < rest; i++)
			*b++ = thiz->last[i];
		thiz->last_offset = rest;
		thiz->curr += 4;
		declen += rest;
	}
	return declen + extra;
}

static void _enesim_stream_base64_reset(void *data)
{
	Enesim_Stream_Base64 *thiz = data;
	thiz->curr = thiz->buf;
	thiz->last_offset = 0;
}

static char * _enesim_stream_base64_location(void *data)
{
	Enesim_Stream_Base64 *thiz = data;
	return enesim_stream_location(thiz->data);
}

static void _enesim_stream_base64_free(void *data)
{
	Enesim_Stream_Base64 *thiz = data;

	enesim_stream_munmap(thiz->data, thiz->buf);
	enesim_stream_free(thiz->data);
	free(thiz);
}

static Enesim_Stream_Descriptor _enesim_stream_base64_descriptor = {
	/* .read	= */ _enesim_stream_base64_read,
	/* .write	= */ NULL, /* not implemented yet */
	/* .mmap	= */ NULL, /* impossible to do */
	/* .munmap	= */ NULL,
	/* .reset	= */ _enesim_stream_base64_reset,
	/* .length	= */ NULL, /* impossible to do */
	/* .location	= */ _enesim_stream_base64_location,
	/* .free	= */ _enesim_stream_base64_free,
};
/*============================================================================*
 *                                   API                                      *
 *============================================================================*/
EAPI Enesim_Stream * enesim_stream_base64_new(Enesim_Stream *d)
{
	Enesim_Stream_Base64 *thiz;
	char *buf;
	size_t size;

	buf = enesim_stream_mmap(d, &size);
	if (!buf) return NULL;

	thiz = calloc(1, sizeof(Enesim_Stream_Base64));
	thiz->data = d;
	thiz->buf = thiz->curr = buf;
	thiz->end = thiz->buf + size;
	thiz->enclen = size;
	thiz->declen = (size / 3) * 4;
	thiz->offset = 0;

	return enesim_stream_new(&_enesim_stream_base64_descriptor, thiz);
}
