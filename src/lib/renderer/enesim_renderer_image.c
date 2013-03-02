/* ENESIM - Direct Rendering Library
 * Copyright (C) 2007-2010 Jorge Luis Zapata
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
#include "libargb.h"

#include "enesim_main.h"
#include "enesim_eina.h"
#include "enesim_error.h"
#include "enesim_color.h"
#include "enesim_rectangle.h"
#include "enesim_matrix.h"
#include "enesim_pool.h"
#include "enesim_buffer.h"
#include "enesim_surface.h"
#include "enesim_compositor.h"
#include "enesim_renderer.h"
#include "enesim_renderer_image.h"

#ifdef BUILD_OPENCL
#include "Enesim_OpenCL.h"
#endif

#ifdef BUILD_OPENGL
#include "Enesim_OpenGL.h"
#endif

#include "enesim_renderer_private.h"
/**
 * @todo
 * - add support for sw and sh
 * - add support for qualities (good scaler, interpolate between the four neighbours)
 * - all the span functions have a memset ... that has to be fixed to:
 *   + whenever the color = 0, use a background renderer and call it directly
 *   + on sw hints check the same condition and use the background renderer hints
 */
/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/
#define ENESIM_LOG_DEFAULT enesim_log_renderer_image

#define ENESIM_RENDERER_IMAGE_MAGIC_CHECK(d)\
	do {\
		if (!EINA_MAGIC_CHECK(d, ENESIM_RENDERER_IMAGE_MAGIC))\
			EINA_MAGIC_FAIL(d, ENESIM_RENDERER_IMAGE_MAGIC);\
	} while(0)

typedef struct _Enesim_Renderer_Image_State
{
	Enesim_Surface *s;
	double x, y;
	double w, h;
} Enesim_Renderer_Image_State;

typedef struct _Enesim_Renderer_Image
{
	EINA_MAGIC
	Enesim_Renderer_Image_State current;
	Enesim_Renderer_Image_State past;
	/* private */
	Enesim_Color color;
	uint32_t *src;
	int sw, sh;
	size_t sstride;
	Eina_F16p16 ixx, iyy;
	Eina_F16p16 iww, ihh;
	Eina_F16p16 mxx, myy;
	Eina_F16p16 nxx, nyy;
	Enesim_F16p16_Matrix matrix;
	Enesim_Compositor_Span span;
	Eina_List *surface_damages;
	Eina_Bool simple : 1;
	Eina_Bool changed : 1;
	Eina_Bool src_changed : 1;
} Enesim_Renderer_Image;


static inline Enesim_Renderer_Image * _image_get(Enesim_Renderer *r)
{
	Enesim_Renderer_Image *thiz;

	thiz = enesim_renderer_data_get(r);
	ENESIM_RENDERER_IMAGE_MAGIC_CHECK(thiz);

	return thiz;
}

static void _image_transform_bounds(Enesim_Renderer *r EINA_UNUSED,
		Enesim_Matrix *tx,
		Enesim_Matrix_Type type,
		Enesim_Rectangle *obounds,
		Enesim_Rectangle *bounds)
{
	*bounds = *obounds;
	/* apply the inverse matrix */
	if (type != ENESIM_MATRIX_IDENTITY)
	{
		Enesim_Quad q;
		Enesim_Matrix m;

		enesim_matrix_inverse(tx, &m);
		enesim_matrix_rectangle_transform(&m, bounds, &q);
		enesim_quad_rectangle_to(&q, bounds);
		/* fix the antialias scaling */
		bounds->x -= m.xx;
		bounds->y -= m.yy;
		bounds->w += m.xx;
		bounds->h += m.yy;
	}

}

static void _image_transform_destination_bounds(Enesim_Renderer *r EINA_UNUSED,
		Enesim_Matrix *tx,
		Enesim_Matrix_Type type,
		Enesim_Rectangle *obounds,
		Eina_Rectangle *bounds)
{
	enesim_rectangle_normalize(obounds, bounds);
}

/* blend simple */
static void _argb8888_blend_span(Enesim_Renderer *r,
		int x, int y, unsigned int len, void *ddata)
{
	Enesim_Renderer_Image *thiz = _image_get(r);
	uint32_t *src = thiz->src;
	uint32_t *dst = ddata;

	x -= eina_f16p16_int_to(thiz->ixx);
	y -= eina_f16p16_int_to(thiz->iyy);

	if ((y < 0) || (y >= thiz->sh) || (x >= thiz->sw) || (x + len < 0))
		return;
	if (x < 0)
	{
		len += x;  dst -= x;  x = 0;
	}
	if (len > thiz->sw - x)
		len = thiz->sw - x;
	src = argb8888_at(src, thiz->sstride, x, y);
	thiz->span(dst, len, src, thiz->color, NULL);
}


/* fast - no pre-scaling */
static void _argb8888_image_no_scale_affine_fast(Enesim_Renderer *r,
		int x, int y, unsigned int len, void *ddata)
{
	Enesim_Renderer_Image *thiz = _image_get(r);
	uint32_t *dst = ddata, *end = dst + len;
	uint32_t *src = thiz->src;
	int sw = thiz->sw, sh = thiz->sh;
	Eina_F16p16 xx, yy;
	Enesim_Color color = thiz->color;

	if (!color)
	{
		memset(dst, 0, sizeof(unsigned int) * len);
		return;
	}
	if (color == 0xffffffff)
		color = 0;

	xx = (thiz->matrix.xx * x) + (thiz->matrix.xx >> 1) +
		(thiz->matrix.xy * y) + (thiz->matrix.xy >> 1) +
		thiz->matrix.xz - 32768 - thiz->ixx;
	yy = (thiz->matrix.yx * x) + (thiz->matrix.yx >> 1) +
		(thiz->matrix.yy * y) + (thiz->matrix.yy >> 1) +
		thiz->matrix.yz - 32768 - thiz->iyy;

	while (dst < end)
	{
		uint32_t p0 = 0;

		x = eina_f16p16_int_to(xx);
		y = eina_f16p16_int_to(yy);

		if (((unsigned)x < sw) & ((unsigned)y < sh))
		{
			p0 = *(src + (y * sw) + x);
			if (color && p0)
				p0 = argb8888_mul4_sym(p0, color);
		}
		*dst++ = p0;  xx += thiz->matrix.xx;  yy += thiz->matrix.yx;
	}
}

static void _argb8888_image_no_scale_projective_fast(Enesim_Renderer *r,
		int x EINA_UNUSED, int y EINA_UNUSED, unsigned int len EINA_UNUSED, void *ddata EINA_UNUSED)
{
	Enesim_Renderer_Image *thiz = _image_get(r);
}

/* fast - pre-scaling */
static void _argb8888_image_scale_identity_fast(Enesim_Renderer *r,
		int x, int y, unsigned int len, void *ddata)
{
	Enesim_Renderer_Image *thiz = _image_get(r);
	uint32_t *dst = ddata, *end = dst + len;
	uint32_t *src = thiz->src;
	int sw = thiz->sw, sh = thiz->sh;
	Eina_F16p16 iww = thiz->iww, ihh = thiz->ihh;
	Eina_F16p16 mxx = thiz->mxx, myy = thiz->myy;
	Eina_F16p16 xx, yy, ixx, iyy;
	int iy;
	Enesim_Color color = thiz->color;

	if (!color)
	{
		memset(dst, 0, sizeof(unsigned int) * len);
		return;
	}
	if (color == 0xffffffff)
		color = 0;

	xx = eina_f16p16_int_from(x) - thiz->ixx;
	yy = eina_f16p16_int_from(y) - thiz->iyy;

	if ((yy < 0) || (yy >= ihh))
	{
		memset(dst, 0, sizeof(unsigned int) * len);
		return;
	}

	iyy = (myy * (long long int)yy) >> 16;
	iy = eina_f16p16_int_to(iyy);
	src += (iy * sw);
	ixx = (mxx * (long long int)xx) >> 16;

	while (dst < end)
	{
		uint32_t p0 = 0;

		if ((xx >= 0) & (xx < iww))
		{
			int ix = eina_f16p16_int_to(ixx);

			p0 = *(src + ix);
			if (p0 && color)
				p0 = argb8888_mul4_sym(p0, color);
        	}
		*dst++ = p0;  xx += EINA_F16P16_ONE;  ixx += mxx;
	}
}

static void _argb8888_image_scale_affine_fast(Enesim_Renderer *r,
		int x, int y, unsigned int len, void *ddata)
{
	Enesim_Renderer_Image *thiz = _image_get(r);
	uint32_t *dst = ddata, *end = dst + len;
	uint32_t *src = thiz->src;
	int sw = thiz->sw, sh = thiz->sh;
	Eina_F16p16 iww = thiz->iww, ihh = thiz->ihh;
	Eina_F16p16 mxx = thiz->mxx, myy = thiz->myy;
	Eina_F16p16 xx, yy;
	Enesim_Color color = thiz->color;

	if (!color)
	{
		memset(dst, 0, sizeof(unsigned int) * len);
		return;
	}
	if (color == 0xffffffff)
		color = 0;

	xx = (thiz->matrix.xx * x) + (thiz->matrix.xx >> 1) +
		(thiz->matrix.xy * y) + (thiz->matrix.xy >> 1) +
		thiz->matrix.xz - 32768 - thiz->ixx;
	yy = (thiz->matrix.yx * x) + (thiz->matrix.yx >> 1) +
		(thiz->matrix.yy * y) + (thiz->matrix.yy >> 1) +
		thiz->matrix.yz - 32768 - thiz->iyy;

	while (dst < end)
	{
		uint32_t p0 = 0;

		if ( ((unsigned)xx < iww) &
			((unsigned)yy < ihh) )
		{
			Eina_F16p16 ixx, iyy;
			int ix, iy;

			ixx = (mxx * (long long int)xx) >> 16;
			ix = eina_f16p16_int_to(ixx);
			iyy = (myy * (long long int)yy) >> 16;
			iy = eina_f16p16_int_to(iyy);

			p0 = *(src + (iy * sw) + ix);

			if (p0 && color)
				p0 = argb8888_mul4_sym(p0, color);
        	}
		*dst++ = p0;  xx += thiz->matrix.xx;  yy += thiz->matrix.yx;
	}
}

static void _argb8888_image_scale_projective_fast(Enesim_Renderer *r,
		int x EINA_UNUSED, int y EINA_UNUSED, unsigned int len EINA_UNUSED, void *ddata EINA_UNUSED)
{
	Enesim_Renderer_Image *thiz = _image_get(r);

}


/* good - no pre-scaling */
static void _argb8888_image_no_scale_identity(Enesim_Renderer *r,
		int x, int y, unsigned int len, void *ddata)
{
	Enesim_Renderer_Image *thiz = _image_get(r);
	uint32_t *src = thiz->src;
	uint32_t *dst = ddata;


	x -= eina_f16p16_int_to(thiz->ixx);
	y -= eina_f16p16_int_to(thiz->iyy);

	if ((y < 0) || (y >= thiz->sh) || (x >= thiz->sw) || (x + len < 0) || !thiz->color)
	{
		memset(dst, 0, sizeof(unsigned int) * len);
		return;
	}
	if (x < 0)
	{
		x = -x;
		memset(dst, 0, sizeof(unsigned int) * x);
		len -= x;  dst += x;  x = 0;
	}
	if (len > thiz->sw - x)
	{
		memset(dst + (thiz->sw - x), 0, sizeof(unsigned int) * (len - (thiz->sw - x)));
		len = thiz->sw - x;
	}
	src = argb8888_at(src, thiz->sstride, x, y);
	thiz->span(dst, len, src, thiz->color, NULL);
}

static void _argb8888_image_no_scale_affine(Enesim_Renderer *r,
		int x, int y, unsigned int len, void *ddata)
{
	Enesim_Renderer_Image *thiz = _image_get(r);
	uint32_t *dst = ddata, *end = dst + len;
	uint32_t *src = thiz->src;
	int sw = thiz->sw, sh = thiz->sh;
	Eina_F16p16 xx, yy;
	Enesim_Color color = thiz->color;

	if (!color)
	{
		memset(dst, 0, sizeof(unsigned int) * len);
		return;
	}
	if (color == 0xffffffff)
		color = 0;

	xx = (thiz->matrix.xx * x) + (thiz->matrix.xx >> 1) +
		(thiz->matrix.xy * y) + (thiz->matrix.xy >> 1) +
		thiz->matrix.xz - 32768 - thiz->ixx;
	yy = (thiz->matrix.yx * x) + (thiz->matrix.yx >> 1) +
		(thiz->matrix.yy * y) + (thiz->matrix.yy >> 1) +
		thiz->matrix.yz - 32768 - thiz->iyy;

	while (dst < end)
	{
		uint32_t p0 = 0;

		x = eina_f16p16_int_to(xx);
		y = eina_f16p16_int_to(yy);

		if ( (((unsigned) (x + 1)) < (sw + 1)) & (((unsigned) (y + 1)) < (sh + 1)) )
		{
			uint32_t *p = src + (y * sw) + x;
			uint32_t p1 = 0, p2 = 0, p3 = 0;

			if ((x > -1) && (y > - 1))
				p0 = *p;
			if ((y > -1) && ((x + 1) < sw))
				p1 = *(p + 1);
			if ((y + 1) < sh)
			{
				if (x > -1)
					p2 = *(p + sw);
				if ((x + 1) < sw)
					p3 = *(p + sw + 1);
			}
			if (p0 | p1 | p2 | p3)
			{
				uint16_t ax = 1 + ((xx & 0xffff) >> 8);
				uint16_t ay = 1 + ((yy & 0xffff) >> 8);

				p0 = argb8888_interp_256(ax, p1, p0);
				p2 = argb8888_interp_256(ax, p3, p2);
				p0 = argb8888_interp_256(ay, p2, p0);
				if (color && p0)
					p0 = argb8888_mul4_sym(p0, color);
			}
		}
		*dst++ = p0;  xx += thiz->matrix.xx;  yy += thiz->matrix.yx;
	}
}

static void _argb8888_image_no_scale_projective(Enesim_Renderer *r,
		int x EINA_UNUSED, int y EINA_UNUSED, unsigned int len EINA_UNUSED, void *ddata EINA_UNUSED)
{
	Enesim_Renderer_Image *thiz = _image_get(r);
}

/* good - pre-scaling */
static void _argb8888_image_scale_identity(Enesim_Renderer *r,
		int x, int y, unsigned int len, void *ddata)
{
	Enesim_Renderer_Image *thiz = _image_get(r);
	uint32_t *dst = ddata, *end = dst + len;
	uint32_t *src = thiz->src;
	int sw = thiz->sw, sh = thiz->sh;
	Eina_F16p16 iww = thiz->iww, ihh = thiz->ihh;
	Eina_F16p16 mxx = thiz->mxx, myy = thiz->myy;
	Eina_F16p16 xx, yy, ixx, iyy;
	int iy;
	uint16_t ay;
	Enesim_Color color = thiz->color;

	if (!color)
	{
		memset(dst, 0, sizeof(unsigned int) * len);
		return;
	}
	if (color == 0xffffffff)
		color = 0;

	xx = eina_f16p16_int_from(x) - thiz->ixx;
	yy = eina_f16p16_int_from(y) - thiz->iyy;

	if ((yy <= -EINA_F16P16_ONE) || (yy >= ihh))
	{
		memset(dst, 0, sizeof(unsigned int) * len);
		return;
	}

	iyy = (myy * (long long int)yy) >> 16;
	iy = eina_f16p16_int_to(iyy);
	src += (iy * sw);
	ay = 1 + ((iyy & 0xffff) >> 8);
	if (yy < 0)
		ay = 1 + ((yy & 0xffff) >> 8);
	if ((ihh - yy) < EINA_F16P16_ONE)
		ay = 256 - ((ihh - yy) >> 8);
	ixx = (mxx * (long long int)xx) >> 16;

	while (dst < end)
	{
		uint32_t p0 = 0;

		if ((xx > -EINA_F16P16_ONE) & (xx < iww))
		{
			int ix = eina_f16p16_int_to(ixx);
			uint32_t *p = src + ix;
			uint32_t p1 = 0, p2 = 0, p3 = 0;

			if ((iy > -1) & (ix > -1))
				p0 = *p;
			if ((iy > -1) & ((ix + 1) < sw))
				p1 = *(p + 1);
			if ((iy + 1) < sh)
			{
				if (ix > -1)
					p2 = *(p + sw);
				if ((ix + 1) < sw)
					p3 = *(p + sw + 1);
			}
			if (p0 | p1 | p2 | p3)
			{
				uint16_t ax = 1 + ((ixx & 0xffff) >> 8);

				if (xx < 0)
					ax = 1 + ((xx & 0xffff) >> 8);
				if ((iww - xx) < EINA_F16P16_ONE)
					ax = 256 - ((iww - xx) >> 8);
				p0 = argb8888_interp_256(ax, p1, p0);
				p2 = argb8888_interp_256(ax, p3, p2);
				p0 = argb8888_interp_256(ay, p2, p0);
				if (color && p0)
					p0 = argb8888_mul4_sym(p0, color);
			}
        	}
		*dst++ = p0;  xx += EINA_F16P16_ONE;  ixx += mxx;
	}
}

static void _argb8888_image_scale_affine(Enesim_Renderer *r,
		int x, int y, unsigned int len, void *ddata)
{
	Enesim_Renderer_Image *thiz = _image_get(r);
	uint32_t *dst = ddata, *end = dst + len;
	uint32_t *src = thiz->src;
	int sw = thiz->sw, sh = thiz->sh;
	Eina_F16p16 iww = thiz->iww, ihh = thiz->ihh;
	Eina_F16p16 mxx = thiz->mxx, myy = thiz->myy;
	Eina_F16p16 xx, yy;
	Enesim_Color color = thiz->color;

	if (!color)
	{
		memset(dst, 0, sizeof(unsigned int) * len);
		return;
	}
	if (color == 0xffffffff)
		color = 0;

	xx = (thiz->matrix.xx * x) + (thiz->matrix.xx >> 1) +
		(thiz->matrix.xy * y) + (thiz->matrix.xy >> 1) +
		thiz->matrix.xz - 32768 - thiz->ixx;
	yy = (thiz->matrix.yx * x) + (thiz->matrix.yx >> 1) +
		(thiz->matrix.yy * y) + (thiz->matrix.yy >> 1) +
		thiz->matrix.yz - 32768 - thiz->iyy;

	while (dst < end)
	{
		uint32_t p0 = 0;

		if ( (((unsigned) (xx + EINA_F16P16_ONE)) < (iww + EINA_F16P16_ONE)) &
			(((unsigned) (yy + EINA_F16P16_ONE)) < (ihh + EINA_F16P16_ONE)) )
		{
			Eina_F16p16 ixx, iyy;
			int ix, iy;
			uint32_t *p, p3 = 0, p2 = 0, p1 = 0;

			ixx = (mxx * (long long int)xx) >> 16;
			ix = eina_f16p16_int_to(ixx);
			iyy = (myy * (long long int)yy) >> 16;
			iy = eina_f16p16_int_to(iyy);

			p = src + (iy * sw) + ix;

			if ((ix > -1) & (iy > -1))
				p0 = *p;
			if ((iy > -1) & ((ix + 1) < sw))
				p1 = *(p + 1);
			if ((iy + 1) < sh)
			{
				if (ix > -1)
					p2 = *(p + sw);
				if ((ix + 1) < sw)
					p3 = *(p + sw + 1);
			}
			if (p0 | p1 | p2 | p3)
			{
				uint16_t ax, ay;

				ax = 1 + ((ixx & 0xffff) >> 8);
				ay = 1 + ((iyy & 0xffff) >> 8);
				if (xx < 0)
					ax = 1 + ((xx & 0xffff) >> 8);
				if ((iww - xx) < EINA_F16P16_ONE)
					ax = 256 - ((iww - xx) >> 8);
				if (yy < 0)
					ay = 1 + ((yy & 0xffff) >> 8);
				if ((ihh - yy) < EINA_F16P16_ONE)
					ay = 256 - ((ihh - yy) >> 8);

				p0 = argb8888_interp_256(ax, p1, p0);
				p2 = argb8888_interp_256(ax, p3, p2);
				p0 = argb8888_interp_256(ay, p2, p0);
				if (color && p0)
					p0 = argb8888_mul4_sym(p0, color);
			}
        	}
		*dst++ = p0;  xx += thiz->matrix.xx;  yy += thiz->matrix.yx;
	}
}

static void _argb8888_image_scale_projective(Enesim_Renderer *r,
		int x EINA_UNUSED, int y EINA_UNUSED, unsigned int len EINA_UNUSED, void *ddata EINA_UNUSED)
{
	Enesim_Renderer_Image *thiz = _image_get(r);

}


/* best - always pre-scales */
static void _argb8888_image_scale_d_u_identity(Enesim_Renderer *r,
		int x, int y, unsigned int len, void *ddata)
{
	Enesim_Renderer_Image *thiz = _image_get(r);
	uint32_t *dst = ddata, *end = dst + len;
	uint32_t *src = thiz->src, *q;
	int sw = thiz->sw, sh = thiz->sh;
	Eina_F16p16 iww = thiz->iww, ihh = thiz->ihh;
	Eina_F16p16 mxx = thiz->mxx, myy = thiz->myy;
	Eina_F16p16 nxx = thiz->nxx;
	Eina_F16p16 xx, yy, ixx, iyy;
	int iy;
	uint16_t ay;
	Enesim_Color color = thiz->color;

	if (!color)
	{
		memset(dst, 0, sizeof(unsigned int) * len);
		return;
	}
	if (color == 0xffffffff)
		color = 0;

	xx = eina_f16p16_int_from(x) - thiz->ixx;
	yy = eina_f16p16_int_from(y) - thiz->iyy;

	if ((yy <= -EINA_F16P16_ONE) || (yy >= ihh))
	{
		memset(dst, 0, sizeof(unsigned int) * len);
		return;
	}

	ixx = (mxx * (long long int)xx) >> 16;
	iyy = (myy * (long long int)yy) >> 16;
	ay = 1 + ((iyy & 0xffff) >> 8);
	if (yy < 0)
		ay = 1 + ((yy & 0xffff) >> 8);
	if ((ihh - yy) < EINA_F16P16_ONE)
		ay = 256 - ((ihh - yy) >> 8);

	iy = iyy >> 16;
	q = src + (iy * sw);
	while (dst < end)
	{
		uint32_t p0 = 0;

		if ((xx > -EINA_F16P16_ONE) & (xx < iww))
		{
			uint32_t ag0 = 0, rb0 = 0;
			int ix, tx, ntx;
			Eina_F16p16 txx, ntxx;
			uint32_t *p;

			x = xx >> 16;  ix = ixx >> 16;
			txx = xx - (xx & 0xffff);  tx = txx >> 16;
			ntxx = txx + nxx;  ntx = ntxx >> 16;
			p = q + ix;
			while (ix < sw)
			{
				uint32_t p1 = 0, p2 = 0;

				if (ix > -1)
				{
					if (iy > -1)
						p1 = *p;
					if (iy + 1 < sh)
						p2 = *(p + sw);
				}
				if (p1 | p2)
				    p1 = argb8888_interp_256(ay, p2, p1);
				if (ntx != tx)
				{
					int ax;

					if (ntx != x)
					{
						ax = 65536 - (txx & 0xffff);
						ag0 += ((((p1 >> 16) & 0xff00) * ax) & 0xffff0000) +
							(((p1 & 0xff00) * ax) >> 16);
						rb0 += ((((p1 >> 8) & 0xff00) * ax) & 0xffff0000) +
							(((p1 & 0xff) * ax) >> 8);
						break;
					}
					ax = 256 + (ntxx & 0xffff);
					ag0 = ((((p1 >> 16) & 0xff00) * ax) & 0xffff0000) +
						(((p1 & 0xff00) * ax) >> 16);
					rb0 = ((((p1 >> 8) & 0xff00) * ax) & 0xffff0000) +
						(((p1 & 0xff) * ax) >> 8);
					tx = ntx;
				}
				else
				{
					ag0 += ((((p1 >> 16) & 0xff00) * nxx) & 0xffff0000) +
						(((p1 & 0xff00) * nxx) >> 16);
					rb0 += ((((p1 >> 8) & 0xff00) * nxx) & 0xffff0000) +
						(((p1 & 0xff) * nxx) >> 8);
				}
				p++;  ix++;
				txx = ntxx;  ntxx += nxx;  ntx = ntxx >> 16;
			}
			p0 = ((ag0 + 0xff00ff) & 0xff00ff00) + (((rb0 + 0xff00ff) >> 8) & 0xff00ff);
			if (color && p0)
				p0 = argb8888_mul4_sym(p0, color);
		}
		*dst++ = p0;  xx += EINA_F16P16_ONE;  ixx += mxx;
	}
}

static void _argb8888_image_scale_u_d_identity(Enesim_Renderer *r,
		int x, int y, unsigned int len, void *ddata)
{
	Enesim_Renderer_Image *thiz = _image_get(r);
	uint32_t *dst = ddata, *end = dst + len;
	uint32_t *src = thiz->src, *q;
	int sw = thiz->sw, sh = thiz->sh;
	Eina_F16p16 iww = thiz->iww, ihh = thiz->ihh;
	Eina_F16p16 mxx = thiz->mxx, myy = thiz->myy;
	Eina_F16p16 nyy = thiz->nyy;
	Eina_F16p16 xx, yy, ixx;
	Eina_F16p16 iyy0, tyy0, ntyy0;
	int iy0, ty0, nty0;
	Enesim_Color color = thiz->color;

	if (!color)
	{
		memset(dst, 0, sizeof(unsigned int) * len);
		return;
	}
	if (color == 0xffffffff)
		color = 0;

	xx = eina_f16p16_int_from(x) - thiz->ixx;
	yy = eina_f16p16_int_from(y) - thiz->iyy;

	if ((yy <= -EINA_F16P16_ONE) || (yy >= ihh))
	{
		memset(dst, 0, sizeof(unsigned int) * len);
		return;
	}

	ixx = (mxx * (long long int)xx) >> 16;
	iyy0 = (myy * (long long int)yy) >> 16;
	iy0 = iyy0 >> 16;
	tyy0 = yy - (yy & 0xffff);  ty0 = tyy0 >> 16;
	ntyy0 = tyy0 + nyy;  nty0 = ntyy0 >> 16;
	q = src + (iy0 * sw);
	y = yy >> 16;

	while (dst < end)
	{
		uint32_t p0 = 0;
		if ((xx > -EINA_F16P16_ONE) & (xx < iww))
		{
			uint32_t ag0 = 0, rb0 = 0;
			int ix, iy = iy0;
			Eina_F16p16 tyy = tyy0, ntyy = ntyy0;
			int ty = ty0, nty = nty0;
			uint32_t *p;
			uint16_t ax;

			ix = ixx >> 16;
			ax = 1 + ((ixx >> 8) & 0xff);
			if (xx < 0)
				ax = 1 + ((xx & 0xffff) >> 8);
			if ((iww - xx) < EINA_F16P16_ONE)
				ax = 256 - ((iww - xx) >> 8);
			p = q + ix;
			while (iy < sh)
			{
				uint32_t p3 = 0, p2 = 0;

				if (iy > -1)
				{
					if (ix > -1)
						p2 = *p;
					if (ix + 1 < sw)
						p3 = *(p + 1);
				}
				if (p2 | p3)
					p2 = argb8888_interp_256(ax, p3, p2);

				if (nty != ty)
				{
					int ay;

					if (nty != y)
					{
						ay = 65536 - (tyy & 0xffff);
						ag0 += ((((p2 >> 16) & 0xff00) * ay) & 0xffff0000) +
							(((p2 & 0xff00) * ay) >> 16);
						rb0 += ((((p2 >> 8) & 0xff00) * ay) & 0xffff0000) +
							(((p2 & 0xff) * ay) >> 8);
						break;
					}
					ay = 256 + (ntyy & 0xffff);
					ag0 += ((((p2 >> 16) & 0xff00) * ay) & 0xffff0000) +
						(((p2 & 0xff00) * ay) >> 16);
					rb0 += ((((p2 >> 8) & 0xff00) * ay) & 0xffff0000) +
						(((p2 & 0xff) * ay) >> 8);
					ty = nty;
				}
				else
				{
					ag0 += ((((p2 >> 16) & 0xff00) * nyy) & 0xffff0000) +
						(((p2 & 0xff00) * nyy) >> 16);
					rb0 += ((((p2 >> 8) & 0xff00) * nyy) & 0xffff0000) +
						(((p2 & 0xff) * nyy) >> 8);
				}

				p += sw;  iy++;
				tyy = ntyy;  ntyy += nyy;  nty = ntyy >> 16;
			}
			p0 = ((ag0 + 0xff00ff) & 0xff00ff00) + (((rb0 + 0xff00ff) >> 8) & 0xff00ff);
			if (color && p0)
				p0 = argb8888_mul4_sym(p0, color);
		}
	*dst++ = p0;  xx += EINA_F16P16_ONE;  ixx += mxx;
	}
}

static void _argb8888_image_scale_d_d_identity(Enesim_Renderer *r,
		int x, int y, unsigned int len, void *ddata)
{
	Enesim_Renderer_Image *thiz = _image_get(r);
	uint32_t *dst = ddata, *end = dst + len;
	uint32_t *src = thiz->src, *q;
	int sw = thiz->sw, sh = thiz->sh;
	Eina_F16p16 iww = thiz->iww, ihh = thiz->ihh;
	Eina_F16p16 mxx = thiz->mxx, myy = thiz->myy;
	Eina_F16p16 nxx = thiz->nxx, nyy = thiz->nyy;
	Eina_F16p16 xx, yy, ixx;
	Eina_F16p16 iyy0, tyy0, ntyy0;
	int iy0, ty0, nty0;
	Enesim_Color color = thiz->color;

	if (!color)
	{
		memset(dst, 0, sizeof(unsigned int) * len);
		return;
	}
	if (color == 0xffffffff)
		color = 0;

	xx = eina_f16p16_int_from(x) - thiz->ixx;
	yy = eina_f16p16_int_from(y) - thiz->iyy;

	if ((yy <= -EINA_F16P16_ONE) || (yy >= ihh))
	{
		memset(dst, 0, sizeof(unsigned int) * len);
		return;
	}

	y = yy >> 16;
	ixx = (mxx * (long long int)xx) >> 16;
	iyy0 = (myy * (long long int)yy) >> 16;  iy0 = iyy0 >> 16;
	tyy0 = yy - (yy & 0xffff);  ty0 = tyy0 >> 16;
	ntyy0 = tyy0 + nyy;  nty0 = ntyy0 >> 16;
	q = src + (iy0 * sw);

	while (dst < end)
	{
		uint32_t p0 = 0;

		if ((xx > -EINA_F16P16_ONE) & (xx < iww))
		{
			uint32_t ag0 = 0, rb0 = 0;
			int ix0 = (ixx >> 16), iy = iy0;
			Eina_F16p16 txx0 = xx - (xx & 0xffff);
			int tx0 = (txx0 >> 16);
			Eina_F16p16 ntxx0 = (txx0 + nxx);
			int ntx0 = (ntxx0 >> 16);
			Eina_F16p16 tyy = tyy0, ntyy = ntyy0;
			int ty = ty0, nty = nty0;
			uint32_t *ps = q;

			x = xx >> 16;
			while (iy < sh)
			{
				uint32_t ag2 = 0, rb2 = 0;
				Eina_F16p16 txx = txx0, ntxx = ntxx0;
				int tx = tx0, ntx = ntx0, ix = ix0;
				uint32_t *p = ps + ix;

				while (ix < sw)
				{
					uint32_t p1 = 0;

					if ((ix > -1) & (iy > -1))
						p1 = *p;

					if (ntx != tx)
					{
						int ax;

						if (ntx != x)
						{
							ax = 65536 - (txx & 0xffff);
							ag2 += ((((p1 >> 16) & 0xff00) * ax) & 0xffff0000) +
								(((p1 & 0xff00) * ax) >> 16);
							rb2 += ((((p1 >> 8) & 0xff00) * ax) & 0xffff0000) +
								(((p1 & 0xff) * ax) >> 8);
							break;
						}
						ax = 256 + (ntxx & 0xffff);
						ag2 = ((((p1 >> 16) & 0xff00) * ax) & 0xffff0000) +
							(((p1 & 0xff00) * ax) >> 16);
						rb2 = ((((p1 >> 8) & 0xff00) * ax) & 0xffff0000) +
							(((p1 & 0xff) * ax) >> 8);
						tx = ntx;
					}
					else
					{
						ag2 += ((((p1 >> 16) & 0xff00) * nxx) & 0xffff0000) +
							(((p1 & 0xff00) * nxx) >> 16);
						rb2 += ((((p1 >> 8) & 0xff00) * nxx) & 0xffff0000) +
							(((p1 & 0xff) * nxx) >> 8);
					}
					p++;  ix++;
					txx = ntxx;  ntxx += nxx;  ntx = ntxx >> 16;
				}

				if (nty != ty)
				{
					int ay;

					if (nty != y)
					{
						ay = 65536 - (tyy & 0xffff);
						ag0 += (((ag2 >> 16) * ay) & 0xffff0000) +
							(((ag2 & 0xffff) * ay) >> 16);
						rb0 += (((rb2 >> 16) * ay) & 0xffff0000) +
							(((rb2 & 0xffff) * ay) >> 16);
						break;
					}
					ay = 256 + (ntyy & 0xffff);
					ag0 = (((ag2 >> 16) * ay) & 0xffff0000) +
						(((ag2 & 0xffff) * ay) >> 16);
					rb0 = (((rb2 >> 16) * ay) & 0xffff0000) +
						(((rb2 & 0xffff) * ay) >> 16);
					ty = nty;
				}
				else
				{
					ag0 += (((ag2 >> 16) * nyy) & 0xffff0000) +
						(((ag2 & 0xffff) * nyy) >> 16);
					rb0 += (((rb2 >>16) * nyy) & 0xffff0000) +
						(((rb2 & 0xffff) * nyy) >> 16);
				}
				ps += sw;  iy++;
				tyy = ntyy;  ntyy += nyy;  nty = ntyy >> 16;
			}
			p0 = ((ag0 + 0xff00ff) & 0xff00ff00) + (((rb0 + 0xff00ff) >> 8) & 0xff00ff);
			if (color && p0)
				p0 = argb8888_mul4_sym(p0, color);
		}
		*dst++ = p0;  xx += EINA_F16P16_ONE;  ixx += mxx;
	}
}

static void _argb8888_image_scale_d_u_affine(Enesim_Renderer *r,
		int x, int y, unsigned int len, void *ddata)
{
	Enesim_Renderer_Image *thiz = _image_get(r);
	uint32_t *dst = ddata, *end = dst + len;
	uint32_t *src = thiz->src;
	int sw = thiz->sw, sh = thiz->sh;
	Eina_F16p16 iww = thiz->iww, ihh = thiz->ihh;
	Eina_F16p16 mxx = thiz->mxx, myy = thiz->myy;
	Eina_F16p16 nxx = thiz->nxx;
	Eina_F16p16 xx, yy;
	Enesim_Color color = thiz->color;

	if (!color)
	{
		memset(dst, 0, sizeof(unsigned int) * len);
		return;
	}
	if (color == 0xffffffff)
		color = 0;

	xx = (thiz->matrix.xx * x) + (thiz->matrix.xx >> 1) +
		(thiz->matrix.xy * y) + (thiz->matrix.xy >> 1) +
		thiz->matrix.xz - 32768 - thiz->ixx;
	yy = (thiz->matrix.yx * x) + (thiz->matrix.yx >> 1) +
		(thiz->matrix.yy * y) + (thiz->matrix.yy >> 1) +
		thiz->matrix.yz - 32768 - thiz->iyy;

	while (dst < end)
	{
		uint32_t p0 = 0;

		if ( (xx > -EINA_F16P16_ONE) & (yy > -EINA_F16P16_ONE) & (xx < iww) & (yy < ihh) )
		{
			uint32_t ag0 = 0, rb0 = 0;
			Eina_F16p16 ixx, iyy, txx, ntxx;
			int ix, iy, tx, ntx;
			uint16_t ay;
			uint32_t *p;

			x = xx >> 16;
			iyy = (myy * (long long int)yy) >> 16;  iy = iyy >> 16;
			ixx = (mxx * (long long int)xx) >> 16;  ix = ixx >> 16;
			txx = xx - (xx & 0xffff);  tx = txx >> 16;
			ntxx = txx + nxx;  ntx = ntxx >> 16;
			ay = 1 + ((iyy & 0xffff) >> 8);

			if (yy < 0)
				ay = 1 + ((yy & 0xffff) >> 8);
			if ((ihh - yy) < EINA_F16P16_ONE)
				ay = 256 - ((ihh - yy) >> 8);
			p = src + (iy * sw) + ix;

			while (ix < sw)
			{
				uint32_t p1 = 0, p2 = 0;

				if (ix > -1)
				{
					if (iy > -1)
						p1 = *p;
					if (iy + 1 < sh)
						p2 = *(p + sw);
				}
				if (p1 | p2)
				    p1 = argb8888_interp_256(ay, p2, p1);
				if (ntx != tx)
				{
					int ax;

					if (ntx != x)
					{
						ax = 65536 - (txx & 0xffff);
						ag0 += ((((p1 >> 16) & 0xff00) * ax) & 0xffff0000) +
							(((p1 & 0xff00) * ax) >> 16);
						rb0 += ((((p1 >> 8) & 0xff00) * ax) & 0xffff0000) +
							(((p1 & 0xff) * ax) >> 8);
						break;
					}
					ax = 256 + (ntxx & 0xffff);
					ag0 = ((((p1 >> 16) & 0xff00) * ax) & 0xffff0000) +
						(((p1 & 0xff00) * ax) >> 16);
					rb0 = ((((p1 >> 8) & 0xff00) * ax) & 0xffff0000) +
						(((p1 & 0xff) * ax) >> 8);
					tx = ntx;
				}
				else
				{
					ag0 += ((((p1 >> 16) & 0xff00) * nxx) & 0xffff0000) +
						(((p1 & 0xff00) * nxx) >> 16);
					rb0 += ((((p1 >> 8) & 0xff00) * nxx) & 0xffff0000) +
						(((p1 & 0xff) * nxx) >> 8);
				}
				p++;  ix++;
				txx = ntxx;  ntxx += nxx;  ntx = ntxx >> 16;
			}
			p0 = ((ag0 + 0xff00ff) & 0xff00ff00) + (((rb0 + 0xff00ff) >> 8) & 0xff00ff);
			if (color && p0)
				p0 = argb8888_mul4_sym(p0, color);
		}
		*dst++ = p0;  xx += thiz->matrix.xx;  yy += thiz->matrix.yx;
	}
}

static void _argb8888_image_scale_u_d_affine(Enesim_Renderer *r,
		int x, int y, unsigned int len, void *ddata)
{
	Enesim_Renderer_Image *thiz = _image_get(r);
	uint32_t *dst = ddata, *end = dst + len;
	uint32_t *src = thiz->src;
	int sw = thiz->sw, sh = thiz->sh;
	Eina_F16p16 iww = thiz->iww, ihh = thiz->ihh;
	Eina_F16p16 mxx = thiz->mxx, myy = thiz->myy;
	Eina_F16p16 nyy = thiz->nyy;
	Eina_F16p16 xx, yy;
	Enesim_Color color = thiz->color;

	if (!color)
	{
		memset(dst, 0, sizeof(unsigned int) * len);
		return;
	}
	if (color == 0xffffffff)
		color = 0;

	xx = (thiz->matrix.xx * x) + (thiz->matrix.xx >> 1) +
		(thiz->matrix.xy * y) + (thiz->matrix.xy >> 1) +
		thiz->matrix.xz - 32768 - thiz->ixx;
	yy = (thiz->matrix.yx * x) + (thiz->matrix.yx >> 1) +
		(thiz->matrix.yy * y) + (thiz->matrix.yy >> 1) +
		thiz->matrix.yz - 32768 - thiz->iyy;

	while (dst < end)
	{
		uint32_t p0 = 0;

		if ( (xx > -EINA_F16P16_ONE) & (yy > -EINA_F16P16_ONE) & (xx < iww) & (yy < ihh) )
		{
			uint32_t ag0 = 0, rb0 = 0;
			int ix, iy, ty, nty;
			Eina_F16p16 ixx, iyy, tyy, ntyy;
			uint32_t *p;
			uint16_t ax;

			y = yy >> 16;
			ixx = (mxx * (long long int)xx) >> 16;  ix = ixx >> 16;
			iyy = (myy * (long long int)yy) >> 16;  iy = iyy >> 16;
			tyy = yy - (yy & 0xffff);  ty = tyy >> 16;
			ntyy = tyy + nyy;  nty = ntyy >> 16;
			ax = 1 + ((ixx & 0xff) >> 8);
			if (xx < 0)
				ax = 1 + ((xx & 0xffff) >> 8);
			if ((iww - xx) < EINA_F16P16_ONE)
				ax = 256 - ((iww - xx) >> 8);
			p = src + (iy * sw) + ix;

			while (iy < sh)
			{
				uint32_t p3 = 0, p2 = 0;

				if (iy > -1)
				{
					if (ix > -1)
						p2 = *p;
					if (ix + 1 < sw)
						p3 = *(p + 1);
				}
				if (p2 | p3)
					p2 = argb8888_interp_256(ax, p3, p2);

				if (nty != ty)
				{
					int ay;

					if (nty != y)
					{
						ay = 65536 - (tyy & 0xffff);
						ag0 += ((((p2 >> 16) & 0xff00) * ay) & 0xffff0000) +
							(((p2 & 0xff00) * ay) >> 16);
						rb0 += ((((p2 >> 8) & 0xff00) * ay) & 0xffff0000) +
							(((p2 & 0xff) * ay) >> 8);
						break;
					}
					ay = 256 + (ntyy & 0xffff);
					ag0 += ((((p2 >> 16) & 0xff00) * ay) & 0xffff0000) +
						(((p2 & 0xff00) * ay) >> 16);
					rb0 += ((((p2 >> 8) & 0xff00) * ay) & 0xffff0000) +
						(((p2 & 0xff) * ay) >> 8);
					ty = nty;
				}
				else
				{
					ag0 += ((((p2 >> 16) & 0xff00) * nyy) & 0xffff0000) +
						(((p2 & 0xff00) * nyy) >> 16);
					rb0 += ((((p2 >> 8) & 0xff00) * nyy) & 0xffff0000) +
						(((p2 & 0xff) * nyy) >> 8);
				}

				p += sw;  iy++;
				tyy = ntyy;  ntyy += nyy;  nty = ntyy >> 16;
			}
			p0 = ((ag0 + 0xff00ff) & 0xff00ff00) + (((rb0 + 0xff00ff) >> 8) & 0xff00ff);
			if (color && p0)
				p0 = argb8888_mul4_sym(p0, color);
		}
	*dst++ = p0;  xx += thiz->matrix.xx;  yy += thiz->matrix.yx;
	}
}

static void _argb8888_image_scale_d_d_affine(Enesim_Renderer *r,
		int x, int y, unsigned int len, void *ddata)
{
	Enesim_Renderer_Image *thiz = _image_get(r);
	uint32_t *dst = ddata, *end = dst + len;
	uint32_t *src = thiz->src;
	int sw = thiz->sw, sh = thiz->sh;
	Eina_F16p16 iww = thiz->iww, ihh = thiz->ihh;
	Eina_F16p16 mxx = thiz->mxx, myy = thiz->myy;
	Eina_F16p16 nxx = thiz->nxx, nyy = thiz->nyy;
	Eina_F16p16 xx, yy;
	Enesim_Color color = thiz->color;

	if (!color)
	{
		memset(dst, 0, sizeof(unsigned int) * len);
		return;
	}
	if (color == 0xffffffff)
		color = 0;

	xx = (thiz->matrix.xx * x) + (thiz->matrix.xx >> 1) +
		(thiz->matrix.xy * y) + (thiz->matrix.xy >> 1) +
		thiz->matrix.xz - 32768 - thiz->ixx;
	yy = (thiz->matrix.yx * x) + (thiz->matrix.yx >> 1) +
		(thiz->matrix.yy * y) + (thiz->matrix.yy >> 1) +
		thiz->matrix.yz - 32768 - thiz->iyy;

	while (dst < end)
	{
		uint32_t p0 = 0;

		if ( (xx > -EINA_F16P16_ONE) & (yy > -EINA_F16P16_ONE) & (xx < iww) & (yy < ihh) )
		{
			uint32_t ag0 = 0, rb0 = 0;
			int ix0, iy, tx0, ntx0, ty, nty;
			Eina_F16p16 ixx, iyy, txx0, ntxx0, tyy, ntyy;
			uint32_t *ps;

			ixx = ((mxx * (long long int)xx) >> 16);  ix0 = (ixx >> 16);
			iyy = ((myy * (long long int)yy) >> 16);  iy = (iyy >> 16);

			ps = src + (iy * sw);

			tyy = yy - (yy & 0xffff);  ty = (tyy >> 16);
			ntyy = tyy + nyy;  nty = (ntyy >> 16);

			txx0 = xx - (xx & 0xffff);  tx0 = (txx0 >> 16);
			ntxx0 = (txx0 + nxx);  ntx0 = (ntxx0 >> 16);

			x = (xx >> 16);
			y = (yy >> 16);

			while (iy < sh)
			{
				uint32_t ag2 = 0, rb2 = 0;
				Eina_F16p16 txx = txx0, ntxx = ntxx0;
				int tx = tx0, ntx = ntx0, ix = ix0;
				uint32_t *p = ps + ix;

				while (ix < sw)
				{
					uint32_t p1 = 0;

					if ((ix > -1) & (iy > -1))
						p1 = *p;

					if (ntx != tx)
					{
						int ax;

						if (ntx != x)
						{
							ax = 65536 - (txx & 0xffff);
							ag2 += ((((p1 >> 16) & 0xff00) * ax) & 0xffff0000) +
								(((p1 & 0xff00) * ax) >> 16);
							rb2 += ((((p1 >> 8) & 0xff00) * ax) & 0xffff0000) +
								(((p1 & 0xff) * ax) >> 8);
							break;
						}
						ax = 256 + (ntxx & 0xffff);
						ag2 = ((((p1 >> 16) & 0xff00) * ax) & 0xffff0000) +
							(((p1 & 0xff00) * ax) >> 16);
						rb2 = ((((p1 >> 8) & 0xff00) * ax) & 0xffff0000) +
							(((p1 & 0xff) * ax) >> 8);
						tx = ntx;
					}
					else
					{
						ag2 += ((((p1 >> 16) & 0xff00) * nxx) & 0xffff0000) +
							(((p1 & 0xff00) * nxx) >> 16);
						rb2 += ((((p1 >> 8) & 0xff00) * nxx) & 0xffff0000) +
							(((p1 & 0xff) * nxx) >> 8);
					}
					p++;  ix++;
					txx = ntxx;  ntxx += nxx;  ntx = ntxx >> 16;
				}

				if (nty != ty)
				{
					int ay;

					if (nty != y)
					{
						ay = 65536 - (tyy & 0xffff);
						ag0 += (((ag2 >> 16) * ay) & 0xffff0000) +
							(((ag2 & 0xffff) * ay) >> 16);
						rb0 += (((rb2 >> 16) * ay) & 0xffff0000) +
							(((rb2 & 0xffff) * ay) >> 16);
						break;
					}
					ay = 256 + (ntyy & 0xffff);
					ag0 = (((ag2 >> 16) * ay) & 0xffff0000) +
						(((ag2 & 0xffff) * ay) >> 16);
					rb0 = (((rb2 >> 16) * ay) & 0xffff0000) +
						(((rb2 & 0xffff) * ay) >> 16);
					ty = nty;
				}
				else
				{
					ag0 += (((ag2 >> 16) * nyy) & 0xffff0000) +
						(((ag2 & 0xffff) * nyy) >> 16);
					rb0 += (((rb2 >>16) * nyy) & 0xffff0000) +
						(((rb2 & 0xffff) * nyy) >> 16);
				}
				ps += sw;  iy++;
				tyy = ntyy;  ntyy += nyy;  nty = ntyy >> 16;
			}
			p0 = ((ag0 + 0xff00ff) & 0xff00ff00) + (((rb0 + 0xff00ff) >> 8) & 0xff00ff);
			if (color && p0)
				p0 = argb8888_mul4_sym(p0, color);
		}
		*dst++ = p0;  xx += thiz->matrix.xx;  yy += thiz->matrix.yx;
	}
}

static void _argb8888_image_scale_d_u_projective(Enesim_Renderer *r,
		int x EINA_UNUSED, int y EINA_UNUSED, unsigned int len EINA_UNUSED, void *ddata EINA_UNUSED)
{
	Enesim_Renderer_Image *thiz = _image_get(r);
}

static void _argb8888_image_scale_u_d_projective(Enesim_Renderer *r,
		int x EINA_UNUSED, int y EINA_UNUSED, unsigned int len EINA_UNUSED, void *ddata EINA_UNUSED)
{
	Enesim_Renderer_Image *thiz = _image_get(r);
}

static void _argb8888_image_scale_d_d_projective(Enesim_Renderer *r,
		int x EINA_UNUSED, int y EINA_UNUSED, unsigned int len EINA_UNUSED, void *ddata EINA_UNUSED)
{
	Enesim_Renderer_Image *thiz = _image_get(r);
}


static void _a8_to_argb8888_noscale(Enesim_Renderer *r,
		int x, int y, unsigned int len, uint32_t *dst)
{
	Enesim_Renderer_Image *thiz = _image_get(r);
	uint8_t *src = (uint8_t *)thiz->src;
	int sw = thiz->sw;

	x -= (thiz->ixx) >> 16;
	y -= (thiz->iyy) >> 16;

	src += (thiz->sstride * y) + x;
	while (len--)
	{
		if (x >= 0 && x < sw)
		{
			uint8_t a = *src;
			*dst = a << 24 | a << 16 | a << 8 | a;
		}
		else
			*dst = 0;
		x++;
		dst++;
		src++;
	}
}

static Enesim_Renderer_Sw_Fill  _spans_best[2][2][ENESIM_MATRIX_TYPES];
static Enesim_Renderer_Sw_Fill  _spans_good[2][ENESIM_MATRIX_TYPES];
static Enesim_Renderer_Sw_Fill  _spans_fast[2][ENESIM_MATRIX_TYPES];

/*----------------------------------------------------------------------------*
 *                      The Enesim's renderer interface                       *
 *----------------------------------------------------------------------------*/
static const char * _image_name(Enesim_Renderer *r EINA_UNUSED)
{
	return "image";
}

static void _image_bounds_get(Enesim_Renderer *r,
		Enesim_Rectangle *rect)
{
	Enesim_Renderer_Image *thiz;
	Enesim_Rectangle obounds;
	Enesim_Matrix m;
	Enesim_Matrix_Type type;

	thiz = _image_get(r);
	if (!thiz->current.s)
	{
		obounds.x = 0;
		obounds.y = 0;
		obounds.w = 0;
		obounds.h = 0;
	}
	else
	{
		double ox;
		double oy;

		enesim_renderer_origin_get(r, &ox, &oy);
		obounds.x = thiz->current.x;
		obounds.y = thiz->current.y;
		obounds.w = thiz->current.w;
		obounds.h = thiz->current.h;
		/* the translate */
		obounds.x += ox;
		obounds.y += oy;
	}
	enesim_renderer_transformation_get(r, &m);
	enesim_renderer_transformation_type_get(r, &type);
	_image_transform_bounds(r, &m, type, &obounds, rect);
}

static void _image_sw_state_cleanup(Enesim_Renderer *r, Enesim_Surface *s EINA_UNUSED)
{
	Enesim_Renderer_Image *thiz;
	Eina_Rectangle *sd;

	thiz = _image_get(r);
	thiz->span = NULL;
	thiz->changed = EINA_FALSE;
	thiz->src_changed = EINA_FALSE;
	if (thiz->current.s)
		enesim_surface_unlock(thiz->current.s);
	thiz->past = thiz->current;
	EINA_LIST_FREE(thiz->surface_damages, sd)
		free(sd);
}

static Eina_Bool _image_sw_state_setup(Enesim_Renderer *r,
		Enesim_Surface *s EINA_UNUSED,
		Enesim_Renderer_Sw_Fill *fill, Enesim_Error **error EINA_UNUSED)
{
	Enesim_Renderer_Image *thiz;
	Enesim_Format fmt;
	Enesim_Matrix m;
	Enesim_Matrix_Type mtype;
	Enesim_Quality quality;
	double x, y, w, h;
	double ox, oy;

	thiz = _image_get(r);
	if (!thiz->current.s)
	{
		WRN("No surface set");
		return EINA_FALSE;
	}

	enesim_surface_lock(thiz->current.s, EINA_FALSE);

	enesim_renderer_color_get(r, &thiz->color);

	enesim_surface_size_get(thiz->current.s, &thiz->sw, &thiz->sh);
	enesim_surface_data_get(thiz->current.s, (void **)(&thiz->src), &thiz->sstride);
	x = thiz->current.x;  y = thiz->current.y;
	w = thiz->current.w;  h = thiz->current.h;

	enesim_renderer_transformation_get(r, &m);
	enesim_matrix_f16p16_matrix_to(&m, &thiz->matrix);
	mtype = enesim_f16p16_matrix_type_get(&thiz->matrix);
	if (mtype != ENESIM_MATRIX_IDENTITY)
	{
		double sx, sy;

		sx = hypot(m.xx, m.yx);
		sy = hypot(m.xy, m.yy);
		if(fabs(1 - sx) < 1/16.0)  sx = 1;
		if(fabs(1 - sy) < 1/16.0)  sy = 1;

		if (sx && sy)
		{
			sx = 1/sx;  sy = 1/sy;
			w *= sx;  h *= sy;  x *= sx;  y *= sy;
			thiz->matrix.xx *= sx; thiz->matrix.xy *= sx; thiz->matrix.xz *= sx;
			thiz->matrix.yx *= sy; thiz->matrix.yy *= sy; thiz->matrix.yz *= sy;
			mtype = enesim_f16p16_matrix_type_get(&thiz->matrix);
		}
	}

	if ((w < 1) || (h < 1) || (thiz->sw < 1) || (thiz->sh < 1))
	{
		WRN("Size too small");
		return EINA_FALSE;
	}

	enesim_renderer_origin_get(r, &ox, &oy);
	thiz->iww = (w * 65536);
	thiz->ihh = (h * 65536);
	thiz->ixx = 65536 * (x + ox);
	thiz->iyy = 65536 * (y + oy);
	thiz->mxx = 65536;  thiz->myy = 65536;
	thiz->nxx = 65536;  thiz->nyy = 65536;

	/* FIXME we need to use the format from the destination surface */
	fmt = ENESIM_FORMAT_ARGB8888;
	enesim_renderer_quality_get(r, &quality);

	if ((fabs(thiz->sw - w) > 1/256.0) || (fabs(thiz->sh - h) > 1/256.0))
	{
		double sx, isx;
		double sy, isy;
		int dx, dy, sw = thiz->sw, sh = thiz->sh;

		dx = (2*w + (1/256.0) < sw ? 1 : 0);
		dy = (2*h + (1/256.0) < sh ? 1 : 0);

		if (dx)
		{ sx = sw;  isx = w; }
		else
		{ sx = (sw > 1 ? sw - 1 : 1);  isx = (w > 1 ? w - 1 : 1); }
		if (dy)
		{ sy = sh;  isy = h; }
		else
		{ sy = (sh > 1 ? sh - 1 : 1);  isy = (h > 1 ? h - 1 : 1); }

		thiz->mxx = (sx * 65536) / isx;
		thiz->myy = (sy * 65536) / isy;
		thiz->nxx = (isx * 65536) / sx;
		thiz->nyy = (isy * 65536) / sy;

		thiz->simple = EINA_FALSE;
		if (mtype == ENESIM_MATRIX_AFFINE)  // in case it's just a translation
		{
			thiz->ixx -= thiz->matrix.xz;  thiz->matrix.xz = 0;
			thiz->iyy -= thiz->matrix.yz;  thiz->matrix.yz = 0;
			mtype = enesim_f16p16_matrix_type_get(&thiz->matrix);
		}

		if (quality == ENESIM_QUALITY_BEST)
			*fill = _spans_best[dx][dy][mtype];
		else if (quality == ENESIM_QUALITY_GOOD)
			*fill = _spans_good[1][mtype];
		else
			*fill = _spans_fast[1][mtype];
	}
	else
	{
		thiz->simple = EINA_TRUE;
		if ((thiz->ixx & 0xffff) || (thiz->iyy & 0xffff))  // non-int translation
		{
			thiz->matrix.xz -= thiz->ixx;  thiz->ixx = 0;
			thiz->matrix.yz -= thiz->iyy;  thiz->iyy = 0;
			mtype = enesim_f16p16_matrix_type_get(&thiz->matrix);
			if (mtype != ENESIM_MATRIX_IDENTITY)
				thiz->simple = EINA_FALSE;
		}

		if (quality == ENESIM_QUALITY_FAST)
			*fill = _spans_fast[0][mtype];
		else
			*fill = _spans_good[0][mtype];
		if (mtype == ENESIM_MATRIX_IDENTITY)
		{
			Enesim_Rop rop;

			enesim_renderer_rop_get(r, &rop);
			thiz->span = enesim_compositor_span_get(rop, &fmt,
				ENESIM_FORMAT_ARGB8888, thiz->color, ENESIM_FORMAT_NONE);
			if (rop == ENESIM_BLEND)
				*fill = _argb8888_blend_span;
		}
	}

	return EINA_TRUE;
}

static void _image_features_get(Enesim_Renderer *r EINA_UNUSED,
		Enesim_Renderer_Feature *features)
{
	*features = ENESIM_RENDERER_FEATURE_TRANSLATE |
			ENESIM_RENDERER_FEATURE_AFFINE |
			ENESIM_RENDERER_FEATURE_PROJECTIVE |
			ENESIM_RENDERER_FEATURE_ARGB8888 |
			ENESIM_RENDERER_FEATURE_QUALITY; 

}

static void _image_sw_image_hints(Enesim_Renderer *r,
		Enesim_Renderer_Sw_Hint *hints)
{
	Enesim_Rop rop;

	*hints = ENESIM_RENDERER_HINT_COLORIZE;
	enesim_renderer_rop_get(r, &rop);
	if (rop != ENESIM_FILL)
	{
		Enesim_Renderer_Image *thiz = _image_get(r);

		if (thiz->span)
			*hints |= ENESIM_RENDERER_HINT_ROP;
	}

}

static Eina_Bool _image_has_changed(Enesim_Renderer *r)
{
	Enesim_Renderer_Image *thiz;

	thiz = _image_get(r);
	if (thiz->src_changed) return EINA_TRUE;
	if (!thiz->changed) return EINA_FALSE;

	if (thiz->current.x != thiz->past.x)
		return EINA_TRUE;
	if (thiz->current.y != thiz->past.y)
		return EINA_TRUE;
	if (thiz->current.w != thiz->past.w)
		return EINA_TRUE;
	if (thiz->current.h != thiz->past.h)
		return EINA_TRUE;
	return EINA_FALSE;
}

static void _image_damages(Enesim_Renderer *r,
		const Eina_Rectangle *old_bounds,
		Enesim_Renderer_Damage_Cb cb, void *data)
{
	Enesim_Renderer_Image *thiz;
	Eina_Rectangle *sd;
	Eina_Rectangle bounds;


	thiz = _image_get(r);
	/* if we have changed just send the previous bounds
	 * and the current one
	 */
	if (enesim_renderer_has_changed(r))
	{
		Enesim_Rectangle curr_bounds;

		cb(r, old_bounds, EINA_TRUE, data);
		_image_bounds_get(r, &curr_bounds);
		enesim_rectangle_normalize(&curr_bounds, &bounds);
		cb(r, &bounds, EINA_FALSE, data);
	}
	/* in other case, send the surface damages tansformed
	 * to destination coordinates
	 */
	else
	{
		Enesim_Matrix m;
		Enesim_Matrix_Type type;
		Eina_List *l;

		enesim_renderer_transformation_get(r, &m);
		enesim_renderer_transformation_type_get(r, &type);
		EINA_LIST_FOREACH(thiz->surface_damages, l, sd)
		{
			Enesim_Rectangle sdd;

			enesim_rectangle_coords_from(&sdd, sd->x, sd->y, sd->w, sd->h);
			/* the coordinates are relative to the image */
			sdd.x += thiz->current.x;
			sdd.y += thiz->current.y;
			/* TODO clip it to the source bounds */
			_image_transform_destination_bounds(r, &m, type, &sdd, &bounds);
			cb(r, &bounds, EINA_FALSE, data);
		}
	}
}

static void _image_free(Enesim_Renderer *r)
{
	Enesim_Renderer_Image *thiz;

	thiz = _image_get(r);
	if (thiz->current.s)
		enesim_surface_unref(thiz->current.s);
	free(thiz);
}

static Enesim_Renderer_Descriptor _descriptor = {
	/* .version = 			*/ ENESIM_RENDERER_API,
	/* .base_name_get = 		*/ _image_name,
	/* .free = 			*/ _image_free,
	/* .bounds_get = 		*/ _image_bounds_get,
	/* .features_get = 		*/ _image_features_get,
	/* .is_inside = 		*/ NULL,
	/* .damages_get = 		*/ _image_damages,
	/* .has_changed = 		*/ _image_has_changed,
	/* .sw_hints_get =		*/ _image_sw_image_hints,
	/* .sw_setup = 			*/ _image_sw_state_setup,
	/* .sw_cleanup = 		*/ _image_sw_state_cleanup,
	/* .opencl_setup =		*/ NULL,
	/* .opencl_kernel_setup =	*/ NULL,
	/* .opencl_cleanup =		*/ NULL,
	/* .opengl_initialize =        	*/ NULL,
	/* .opengl_setup =          	*/ NULL,
	/* .opengl_cleanup =        	*/ NULL
};
/*============================================================================*
 *                                 Global                                     *
 *============================================================================*/
/*============================================================================*
 *                                   API                                      *
 *============================================================================*/
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI Enesim_Renderer * enesim_renderer_image_new(void)
{
	Enesim_Renderer *r;
	Enesim_Renderer_Image *thiz;
	static Eina_Bool spans_initialized = EINA_FALSE;

	if (!spans_initialized)
	{
		spans_initialized = EINA_TRUE;

		_spans_best[0][0][ENESIM_MATRIX_IDENTITY] = _argb8888_image_scale_identity;
		_spans_best[0][0][ENESIM_MATRIX_AFFINE] = _argb8888_image_scale_affine;
		_spans_best[0][0][ENESIM_MATRIX_PROJECTIVE] = _argb8888_image_scale_projective;
		_spans_best[1][0][ENESIM_MATRIX_IDENTITY] = _argb8888_image_scale_d_u_identity;
		_spans_best[1][0][ENESIM_MATRIX_AFFINE] = _argb8888_image_scale_d_u_affine;
		_spans_best[1][0][ENESIM_MATRIX_PROJECTIVE] = _argb8888_image_scale_d_u_projective;
		_spans_best[0][1][ENESIM_MATRIX_IDENTITY] = _argb8888_image_scale_u_d_identity;
		_spans_best[0][1][ENESIM_MATRIX_AFFINE] = _argb8888_image_scale_u_d_affine;
		_spans_best[0][1][ENESIM_MATRIX_PROJECTIVE] = _argb8888_image_scale_u_d_projective;
		_spans_best[1][1][ENESIM_MATRIX_IDENTITY] = _argb8888_image_scale_d_d_identity;
		_spans_best[1][1][ENESIM_MATRIX_AFFINE] = _argb8888_image_scale_d_d_affine;
		_spans_best[1][1][ENESIM_MATRIX_PROJECTIVE] = _argb8888_image_scale_d_d_projective;

		_spans_good[0][ENESIM_MATRIX_IDENTITY] = _argb8888_image_no_scale_identity;
		_spans_good[0][ENESIM_MATRIX_AFFINE] = _argb8888_image_no_scale_affine;
		_spans_good[0][ENESIM_MATRIX_PROJECTIVE] = _argb8888_image_no_scale_projective;
		_spans_good[1][ENESIM_MATRIX_IDENTITY] = _argb8888_image_scale_identity;
		_spans_good[1][ENESIM_MATRIX_AFFINE] = _argb8888_image_scale_affine;
		_spans_good[1][ENESIM_MATRIX_PROJECTIVE] = _argb8888_image_scale_projective;

		_spans_fast[0][ENESIM_MATRIX_IDENTITY] = _argb8888_image_no_scale_identity;
		_spans_fast[0][ENESIM_MATRIX_AFFINE] = _argb8888_image_no_scale_affine_fast;
		_spans_fast[0][ENESIM_MATRIX_PROJECTIVE] = _argb8888_image_no_scale_projective_fast;
		_spans_fast[1][ENESIM_MATRIX_IDENTITY] = _argb8888_image_scale_identity_fast;
		_spans_fast[1][ENESIM_MATRIX_AFFINE] = _argb8888_image_scale_affine_fast;
		_spans_fast[1][ENESIM_MATRIX_PROJECTIVE] = _argb8888_image_scale_projective_fast;
	}

	thiz = calloc(1, sizeof(Enesim_Renderer_Image));
	if (!thiz) return NULL;

	EINA_MAGIC_SET(thiz, ENESIM_RENDERER_IMAGE_MAGIC);
	r = enesim_renderer_new(&_descriptor, thiz);

	return r;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_image_x_set(Enesim_Renderer *r, double x)
{
	Enesim_Renderer_Image *thiz;

	thiz = _image_get(r);
	if (!thiz) return;
	thiz->current.x = x;
	thiz->changed = EINA_TRUE;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_image_x_get(Enesim_Renderer *r, double *x)
{
	Enesim_Renderer_Image *thiz;

	thiz = _image_get(r);
	if (!thiz) return;
	if (x) *x = thiz->current.x;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_image_y_set(Enesim_Renderer *r, double y)
{
	Enesim_Renderer_Image *thiz;

	thiz = _image_get(r);
	if (!thiz) return;
	thiz->current.y = y;
	thiz->changed = EINA_TRUE;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_image_y_get(Enesim_Renderer *r, double *y)
{
	Enesim_Renderer_Image *thiz;

	thiz = _image_get(r);
	if (!thiz) return;
	if (y) *y = thiz->current.y;
}
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_image_position_set(Enesim_Renderer *r, double x, double y)
{
	Enesim_Renderer_Image *thiz;

	thiz = _image_get(r);
	if (!thiz) return;
	thiz->current.x = x;
	thiz->current.y = y;
	thiz->changed = EINA_TRUE;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_image_position_get(Enesim_Renderer *r, double *x, double *y)
{
	Enesim_Renderer_Image *thiz;

	thiz = _image_get(r);
	if (!thiz) return;
	if (x) *x = thiz->current.x;
	if (y) *y = thiz->current.y;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_image_width_set(Enesim_Renderer *r, double w)
{
	Enesim_Renderer_Image *thiz;

	thiz = _image_get(r);
	if (!thiz) return;
	thiz->current.w = w;
	thiz->changed = EINA_TRUE;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_image_width_get(Enesim_Renderer *r, double *w)
{
	Enesim_Renderer_Image *thiz;

	thiz = _image_get(r);
	if (!thiz) return;
	if (w) *w = thiz->current.w;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_image_height_set(Enesim_Renderer *r, double h)
{
	Enesim_Renderer_Image *thiz;

	thiz = _image_get(r);
	if (!thiz) return;
	thiz->current.h = h;
	thiz->changed = EINA_TRUE;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_image_height_get(Enesim_Renderer *r, double *h)
{
	Enesim_Renderer_Image *thiz;

	thiz = _image_get(r);
	if (!thiz) return;
	if (h) *h = thiz->current.h;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_image_size_set(Enesim_Renderer *r, double w, double h)
{
	Enesim_Renderer_Image *thiz;

	thiz = _image_get(r);
	if (!thiz) return;
	thiz->current.w = w;
	thiz->current.h = h;
	thiz->changed = EINA_TRUE;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_image_size_get(Enesim_Renderer *r, double *w, double *h)
{
	Enesim_Renderer_Image *thiz;

	thiz = _image_get(r);
	if (!thiz) return;
	if (w) *w = thiz->current.w;
	if (h) *h = thiz->current.h;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_image_src_set(Enesim_Renderer *r, Enesim_Surface *src)
{
	Enesim_Renderer_Image *thiz;

	thiz = _image_get(r);
	if (!thiz) return;

	if (thiz->current.s)
		enesim_surface_unref(thiz->current.s);
	thiz->current.s = src;
	if (thiz->current.s)
		thiz->current.s = enesim_surface_ref(thiz->current.s);
	thiz->src_changed = EINA_TRUE;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_image_src_get(Enesim_Renderer *r, Enesim_Surface **src)
{
	Enesim_Renderer_Image *thiz;

	thiz = _image_get(r);
	if (!thiz) return;
	if (*src)
		*src = thiz->current.s;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_image_damage_add(Enesim_Renderer *r, Eina_Rectangle *area)
{
	Enesim_Renderer_Image *thiz;
	Eina_Rectangle *d;

	thiz = _image_get(r);

	d = calloc(1, sizeof(Eina_Rectangle));
	*d = *area;
	thiz->surface_damages = eina_list_append(thiz->surface_damages, d);
}
