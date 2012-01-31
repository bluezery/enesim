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
#include "Enesim.h"
#include "enesim_private.h"
/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/
/* TODO merge this with the bifigure */
#define MUL4_SYM(x, y) \
 ( ((((((x) >> 16) & 0xff00) * (((y) >> 16) & 0xff00)) + 0xff0000) & 0xff000000) + \
   ((((((x) >> 8) & 0xff00) * (((y) >> 16) & 0xff)) + 0xff00) & 0xff0000) + \
   ((((((x) & 0xff00) * ((y) & 0xff00)) + 0xff00) >> 16) & 0xff00) + \
   (((((x) & 0xff) * ((y) & 0xff)) + 0xff) >> 8) )

#define INTERP_65536(a, c0, c1) \
	( ((((((c0 >> 16) & 0xff00) - ((c1 >> 16) & 0xff00)) * a) + \
	  (c1 & 0xff000000)) & 0xff000000) + \
	  ((((((c0 >> 16) & 0xff) - ((c1 >> 16) & 0xff)) * a) + \
	  (c1 & 0xff0000)) & 0xff0000) + \
	  ((((((c0 & 0xff00) - (c1 & 0xff00)) * a) >> 16) + \
	  (c1 & 0xff00)) & 0xff00) + \
	  ((((((c0 & 0xff) - (c1 & 0xff)) * a) >> 16) + \
	  (c1 & 0xff)) & 0xff) )

#define MUL_A_65536(a, c) \
	( ((((c >> 16) & 0xff00) * a) & 0xff000000) + \
	  ((((c >> 16) & 0xff) * a) & 0xff0000) + \
	  ((((c & 0xff00) * a) >> 16) & 0xff00) + \
	  ((((c & 0xff) * a) >> 16) & 0xff) )


#define ENESIM_RASTERIZER_BIFIGURE_MAGIC_CHECK(d) \
	do {\
		if (!EINA_MAGIC_CHECK(d, ENESIM_RASTERIZER_BIFIGURE_MAGIC))\
			EINA_MAGIC_FAIL(d, ENESIM_RASTERIZER_BIFIGURE_MAGIC);\
	} while(0)


typedef struct _Enesim_Rasterizer_BiFigure
{
	EINA_MAGIC
	Enesim_Renderer *over;
	Enesim_Renderer *under;
	Enesim_Figure *over_figure;
	const Enesim_Figure *under_figure;

	int tyy, byy;

	Enesim_F16p16_Matrix matrix;
	Eina_Bool changed :1;
} Enesim_Rasterizer_BiFigure;

static inline Enesim_Rasterizer_BiFigure * _bifigure_get(Enesim_Renderer *r)
{
	Enesim_Rasterizer_BiFigure *thiz;

	thiz = enesim_rasterizer_data_get(r);
	ENESIM_RASTERIZER_BIFIGURE_MAGIC_CHECK(thiz);

	return thiz;
}

#define SETUP_EDGES \
	oedges = alloca(novectors * sizeof(Enesim_F16p16_Edge)); \
	oedge = oedges; \
	while (n < novectors) \
	{ \
		int yy0 = ov->yy0, yy1 = ov->yy1; \
 \
		if (yy1 < yy0) \
		{ \
			yy0 = yy1; \
			yy1 = ov->yy0; \
		} \
		if (yy + 0xffff < yy0) \
			break; \
		if ((((yy + 0xffff)) >= (yy0)) & ((yy) <= ((yy1 + 0xffff)))) \
		{ \
			int xx0 = ov->xx0, xx1 = ov->xx1; \
 \
			if (xx1 < xx0) \
			{ \
				xx0 = xx1; \
				xx1 = ov->xx0; \
			} \
			oedge->xx0 = xx0; \
			oedge->xx1 = xx1; \
			oedge->yy0 = yy0; \
			oedge->yy1 = yy1; \
			oedge->de = (ov->a * (long long int) axx) >> 16; \
			oedge->e = ((ov->a * (long long int) xx) >> 16) + \
					((ov->b * (long long int) yy) >> 16) + \
					ov->c; \
			if (lx > xx0)  lx = xx0; \
			if (rx < xx1)  rx = xx1; \
			oedge++; \
			noedges++; \
		} \
		n++; \
		ov++; \
	} \
 \
	uedges = alloca(nuvectors * sizeof(Enesim_F16p16_Edge)); \
	uedge = uedges; \
	while (m < nuvectors) \
	{ \
		int yy0 = uv->yy0, yy1 = uv->yy1; \
 \
		if (yy1 < yy0) \
		{ \
			yy0 = yy1; \
			yy1 = uv->yy0; \
		} \
		if (yy + 0xffff < yy0) \
			break; \
		if ((((yy + 0xffff)) >= (yy0)) & ((yy) <= ((yy1 + 0xffff)))) \
		{ \
			int xx0 = uv->xx0, xx1 = uv->xx1; \
 \
			if (xx1 < xx0) \
			{ \
				xx0 = xx1; \
				xx1 = uv->xx0; \
			} \
			uedge->xx0 = xx0; \
			uedge->xx1 = xx1; \
			uedge->yy0 = yy0; \
			uedge->yy1 = yy1; \
			uedge->de = (uv->a * (long long int) axx) >> 16; \
			uedge->e = ((uv->a * (long long int) xx) >> 16) + \
					((uv->b * (long long int) yy) >> 16) + \
					uv->c; \
			if (lx > xx0)  lx = xx0; \
			if (rx < xx1)  rx = xx1; \
			uedge++; \
			nuedges++; \
		} \
		m++; \
		uv++; \
	} \
	if (!noedges && !nuedges) \
		goto get_out; \
 \
	lx = (lx >> 16) - 2 - x; \
	if (lx > 0) \
	{ \
		memset(dst, 0, sizeof(unsigned int) * lx); \
		xx += lx * axx; \
		d += lx; \
		n = 0;  oedge = oedges; \
		while (n < noedges) \
		{ \
			oedge->e += lx * oedge->de; \
			oedge++; \
			n++; \
		} \
		m = 0;  uedge = uedges; \
		while (m < nuedges) \
		{ \
			uedge->e += lx * uedge->de; \
			uedge++; \
			m++; \
		} \
	} \
	else lx = 0; \
 \
	rx = (rx >> 16) + 2 - x; \
	if (len > rx) \
	{ \
		len -= rx; \
		memset(dst + rx, 0, sizeof(unsigned int) * len); \
		e -= len; \
	} \
	else rx = len;


#define EVAL_EDGES \
		n = 0; \
		uedge = uedges; \
		while (n < nuedges) \
		{ \
			int ee = uedge->e; \
 \
			if ((yy >= uedge->yy0) & (yy < uedge->yy1)) \
			{ \
				ucount++; \
				if (ee < 0) \
					ucount -= 2; \
			} \
			if (ee < 0) \
				ee = -ee; \
 \
			if ((ee < 65536) && ((xx + 0xffff) >= uedge->xx0) & \
					(xx <= (0xffff + uedge->xx1))) \
			{ \
				if (ua < 16384) \
					ua = 65536 - ee; \
				else \
					ua = (ua + (65536 - ee)) / 2; \
			} \
 \
			uedge->e += uedge->de; \
			uedge++; \
			n++; \
		} \
 \
		m = 0; \
		oedge = oedges; \
		while (m < noedges) \
		{ \
			int ee = oedge->e; \
 \
			if ((yy >= oedge->yy0) & (yy < oedge->yy1)) \
			{ \
				ocount++; \
				if (ee < 0) \
					ocount -= 2; \
			} \
			if (ee < 0) \
				ee = -ee; \
 \
			if ((ee < 65536) && ((xx + 0xffff) >= oedge->xx0) & \
					(xx <= (0xffff + oedge->xx1))) \
			{ \
				if (oa < 16384) \
					oa = 65536 - ee; \
				else \
					oa = (oa + (65536 - ee)) / 2; \
			} \
 \
			oedge->e += oedge->de; \
			oedge++; \
			m++; \
		}


static void _bifig_stroke_fill_paint_affine_simple(Enesim_Renderer *r,
		const Enesim_Renderer_State *state,
		int x, int y,
		unsigned int len, void *ddata)
{
	Enesim_Rasterizer_BiFigure *thiz = _bifigure_get(r);
	Enesim_Color color;
	Enesim_Color fcolor;
	Enesim_Color scolor;
	Enesim_Renderer *fpaint;
	uint32_t *dst = ddata;
	unsigned int *d = dst, *e = d + len;

	Enesim_F16p16_Edge *oedges, *oedge;
	Enesim_F16p16_Vector *ov;
	int novectors, n = 0, noedges = 0;

	Enesim_F16p16_Edge *uedges, *uedge;
	Enesim_F16p16_Vector *uv;
	int nuvectors, m = 0, nuedges = 0;

	double ox, oy;
	int lx = 1<<30, rx = -lx;

	int axx = thiz->matrix.xx, axy = thiz->matrix.xy, axz =
			thiz->matrix.xz;
	int ayy = thiz->matrix.yy, ayz = thiz->matrix.yz;
	int xx = (axx * x) + (axx >> 1) + (axy * y) + (axy >> 1) + axz - 32768;
	int yy = (ayy * y) + (ayy >> 1) + ayz - 32768;

	enesim_rasterizer_basic_vectors_get(thiz->under, &nuvectors, &uv);
	enesim_rasterizer_basic_vectors_get(thiz->over, &novectors, &ov);

	enesim_renderer_origin_get(r, &ox, &oy);
	xx -= eina_f16p16_double_from(ox);
	yy -= eina_f16p16_double_from(oy);

	if ((((yy >> 16) + 1) < (thiz->tyy >> 16)) ||
			((yy >> 16) > (1 + (thiz->byy >> 16))))
	{
get_out:
		memset(d, 0, sizeof(unsigned int) * len);
		return;
	}

	SETUP_EDGES

	enesim_renderer_shape_stroke_color_get(r, &scolor);
	enesim_renderer_shape_fill_color_get(r, &fcolor);
	enesim_renderer_shape_fill_renderer_get(r, &fpaint);

	enesim_renderer_color_get(r, &color);
	if (color != 0xffffffff)
	{
		scolor = argb8888_mul4_sym(color, scolor);
		fcolor = argb8888_mul4_sym(color, fcolor);
	}
	if (fpaint)
	{
		enesim_renderer_sw_draw(fpaint, x + lx, y, rx - lx, dst + lx);
	}

	while (d < e)
	{
		unsigned int p0 = 0;
		int ucount = 0, ocount = 0;
		int ua = 0, oa = 0;

		EVAL_EDGES

		if (ocount)  // inside over figure
			p0 = scolor;
		else if (oa)  // on the outside boundary of the over figure
		{
			unsigned int q0 = 0;

			if (ucount) // inside under figure
			{
				q0 = fcolor;
				if (fpaint)
				{
					q0 = *d;
					if (fcolor != 0xffffffff)
						q0 = MUL4_SYM(fcolor, q0);
				}
			}
			else if (ua) // on the outside boundary of the under figure
			{
				q0 = fcolor;
				if (fpaint)
				{
					q0 = *d;
					if (fcolor != 0xffffffff)
						q0 = MUL4_SYM(fcolor, q0);
				}
				if (ua < 65536)
					q0 = MUL_A_65536(ua, q0);
			}

			if (oa < 65536)
				p0 = INTERP_65536(oa, scolor, q0);
		}
		else // outside over figure and not on its boundary
		{
			if (ucount)  // inside under figure
			{
				p0 = fcolor;
				if (fpaint)
				{
					p0 = *d;
					if (fcolor != 0xffffffff)
						p0 = MUL4_SYM(fcolor, p0);
				}
			}
			else if (ua) // on the outside boundary of the under figure
			{
				p0 = fcolor;
				if (fpaint)
				{
					p0 = *d;
					if (fcolor != 0xffffffff)
						p0 = MUL4_SYM(fcolor, p0);
				}
				if (ua < 65536)
					p0 = MUL_A_65536(ua, p0);
			}
		}
		*d++ = p0;
		xx += axx;
	}
}

static void _bifig_stroke_paint_fill_affine_simple(Enesim_Renderer *r,
		const Enesim_Renderer_State *state,
		int x, int y,
		unsigned int len, void *ddata)
{
	Enesim_Rasterizer_BiFigure *thiz = _bifigure_get(r);
	Enesim_Color color;
	Enesim_Color fcolor;
	Enesim_Color scolor;
	Enesim_Renderer *spaint;
	uint32_t *dst = ddata;
	unsigned int *d = dst, *e = d + len;

	Enesim_F16p16_Edge *oedges, *oedge;
	Enesim_F16p16_Vector *ov;
	int novectors, n = 0, noedges = 0;

	Enesim_F16p16_Edge *uedges, *uedge;
	Enesim_F16p16_Vector *uv;
	int nuvectors, m = 0, nuedges = 0;

	double ox, oy;
	int lx = 1<<30, rx = -lx;

	int axx = thiz->matrix.xx, axy = thiz->matrix.xy, axz =
			thiz->matrix.xz;
	int ayy = thiz->matrix.yy, ayz = thiz->matrix.yz;
	int xx = (axx * x) + (axx >> 1) + (axy * y) + (axy >> 1) + axz - 32768;
	int yy = (ayy * y) + (ayy >> 1) + ayz - 32768;

	enesim_rasterizer_basic_vectors_get(thiz->under, &nuvectors, &uv);
	enesim_rasterizer_basic_vectors_get(thiz->over, &novectors, &ov);

	enesim_renderer_origin_get(r, &ox, &oy);
	xx -= eina_f16p16_double_from(ox);
	yy -= eina_f16p16_double_from(oy);

	if ((((yy >> 16) + 1) < (thiz->tyy >> 16)) ||
			((yy >> 16) > (1 + (thiz->byy >> 16))))
	{
get_out:
		memset(d, 0, sizeof(unsigned int) * len);
		return;
	}

	SETUP_EDGES

	enesim_renderer_shape_stroke_color_get(r, &scolor);
	enesim_renderer_shape_stroke_renderer_get(r, &spaint);
	enesim_renderer_shape_fill_color_get(r, &fcolor);

	enesim_renderer_color_get(r, &color);
	if (color != 0xffffffff)
	{
		scolor = argb8888_mul4_sym(color, scolor);
		fcolor = argb8888_mul4_sym(color, fcolor);
	}

	enesim_renderer_sw_draw(spaint, x + lx, y, rx - lx, dst + lx);

	while (d < e)
	{
		unsigned int p0 = 0;
		int ucount = 0, ocount = 0;
		int ua = 0, oa = 0;

		EVAL_EDGES

		if (ocount)  // inside over figure
		{
			p0 = *d;
			if (scolor != 0xffffffff)
				p0 = MUL4_SYM(scolor, p0);
		}
		else if (oa)  // on the outside boundary of the over figure
		{
			unsigned int q0 = 0;

			if (ucount) // inside under figure
				q0 = fcolor;
			else if (ua) // on the outside boundary of the under figure
			{
				q0 = fcolor;
				if (ua < 65536)
					q0 = MUL_A_65536(ua, q0);
			}

			if (oa < 65536)
			{
				color = *d;
				if (scolor != 0xffffffff)
					color = MUL4_SYM(scolor, color);
				p0 = INTERP_65536(oa, color, q0);
			}
		}
		else // outside over figure and not on its boundary
		{
			if (ucount)  // inside under figure
				p0 = fcolor;
			else if (ua) // on the outside boundary of the under figure
			{
				p0 = fcolor;
				if (ua < 65536)
					p0 = MUL_A_65536(ua, p0);
			}
		}
		*d++ = p0;
		xx += axx;
	}
}

static void _bifig_stroke_paint_fill_paint_affine_simple(Enesim_Renderer *r,
		const Enesim_Renderer_State *state,
		int x, int y,
		unsigned int len, void *ddata)
{
	Enesim_Rasterizer_BiFigure *thiz = _bifigure_get(r);
	Enesim_Color color;
	Enesim_Color fcolor;
	Enesim_Color scolor;
	Enesim_Renderer *fpaint, *spaint;
	uint32_t *dst = ddata;
	unsigned int *d = dst, *e = d + len;
	unsigned int *sbuf, *s;

	Enesim_F16p16_Edge *oedges, *oedge;
	Enesim_F16p16_Vector *ov;
	int novectors, n = 0, noedges = 0;

	Enesim_F16p16_Edge *uedges, *uedge;
	Enesim_F16p16_Vector *uv;
	int nuvectors, m = 0, nuedges = 0;

	double ox, oy;
	int lx = 1<<30, rx = -lx;

	int axx = thiz->matrix.xx, axy = thiz->matrix.xy, axz =
			thiz->matrix.xz;
	int ayy = thiz->matrix.yy, ayz = thiz->matrix.yz;
	int xx = (axx * x) + (axx >> 1) + (axy * y) + (axy >> 1) + axz - 32768;
	int yy = (ayy * y) + (ayy >> 1) + ayz - 32768;

	enesim_rasterizer_basic_vectors_get(thiz->under, &nuvectors, &uv);
	enesim_rasterizer_basic_vectors_get(thiz->over, &novectors, &ov);

	enesim_renderer_origin_get(r, &ox, &oy);
	xx -= eina_f16p16_double_from(ox);
	yy -= eina_f16p16_double_from(oy);

	if ((((yy >> 16) + 1) < (thiz->tyy >> 16)) ||
			((yy >> 16) > (1 + (thiz->byy >> 16))))
	{
get_out:
		memset(d, 0, sizeof(unsigned int) * len);
		return;
	}

	SETUP_EDGES

	enesim_renderer_shape_stroke_color_get(r, &scolor);
	enesim_renderer_shape_stroke_renderer_get(r, &spaint);
	enesim_renderer_shape_fill_color_get(r, &fcolor);
	enesim_renderer_shape_fill_renderer_get(r, &fpaint);

	enesim_renderer_color_get(r, &color);
	if (color != 0xffffffff)
	{
		scolor = argb8888_mul4_sym(color, scolor);
		fcolor = argb8888_mul4_sym(color, fcolor);
	}

	sbuf = alloca((rx - lx) * sizeof(unsigned int));
	enesim_renderer_sw_draw(spaint, x + lx, y, rx - lx, sbuf);
	s = sbuf;

	enesim_renderer_sw_draw(fpaint, x + lx, y, rx - lx, dst + lx);

	while (d < e)
	{
		unsigned int p0 = 0;
		int ucount = 0, ocount = 0;
		int ua = 0, oa = 0;

		EVAL_EDGES

		if (ocount)  // inside over figure
		{
			p0 = *s;
			if (scolor != 0xffffffff)
				p0 = MUL4_SYM(scolor, p0);
		}
		else if (oa)  // on the outside boundary of the over figure
		{
			unsigned int q0 = 0;

			if (ucount) // inside under figure
			{
				q0 = *d;
				if (fcolor != 0xffffffff)
					q0 = MUL4_SYM(fcolor, q0);
			}
			else if (ua) // on the outside boundary of the under figure
			{
				q0 = *d;
				if (fcolor != 0xffffffff)
					q0 = MUL4_SYM(fcolor, q0);
				if (ua < 65536)
					q0 = MUL_A_65536(ua, q0);
			}

			if (oa < 65536)
			{
				color = *s;
				if (scolor != 0xffffffff)
					color = MUL4_SYM(scolor, color);
				p0 = INTERP_65536(oa, color, q0);
			}
		}
		else // outside over figure and not on its boundary
		{
			if (ucount)  // inside under figure
			{
				p0 = *d;
				if (fcolor != 0xffffffff)
					p0 = MUL4_SYM(fcolor, p0);
			}
			else if (ua) // on the outside boundary of the under figure
			{
				p0 = *d;
				if (fcolor != 0xffffffff)
					p0 = MUL4_SYM(fcolor, p0);
				if (ua < 65536)
					p0 = MUL_A_65536(ua, p0);
			}
		}
		*d++ = p0;
		s++;
		xx += axx;
	}
}


static void _over_figure_span(Enesim_Renderer *r,
		const Enesim_Renderer_State *state,
		int x, int y, unsigned int len, void *ddata)
{
	Enesim_Rasterizer_BiFigure *thiz;

	thiz = _bifigure_get(r);
	enesim_renderer_sw_draw(thiz->over, x, y, len, ddata);
}

static void _under_figure_span(Enesim_Renderer *r,
		const Enesim_Renderer_State *state,
		int x, int y, unsigned int len, void *ddata)
{
	Enesim_Rasterizer_BiFigure *thiz;

	thiz = _bifigure_get(r);
	enesim_renderer_sw_draw(thiz->under, x, y, len, ddata);
}
/*----------------------------------------------------------------------------*
 *                    The Enesim's rasterizer interface                       *
 *----------------------------------------------------------------------------*/
static const char * _bifigure_name(Enesim_Renderer *r)
{
	return "bifigure";
}

static void _bifigure_free(Enesim_Renderer *r)
{
	Enesim_Rasterizer_BiFigure *thiz;

	thiz = _bifigure_get(r);
	if (thiz->over)
		enesim_renderer_unref(thiz->over);
	if (thiz->under)
		enesim_renderer_unref(thiz->under);
	free(thiz);
}

static void _bifigure_figure_set(Enesim_Renderer *r, const Enesim_Figure *figure)
{
	Enesim_Rasterizer_BiFigure *thiz;

	thiz = _bifigure_get(r);
	thiz->under_figure = figure;
	if (figure && !thiz->under)
		thiz->under = enesim_rasterizer_basic_new();
	thiz->changed = EINA_TRUE;
}

static Eina_Bool _bifigure_sw_setup(Enesim_Renderer *r,
		const Enesim_Renderer_State *states[ENESIM_RENDERER_STATES],
		Enesim_Surface *s,
		Enesim_Renderer_Sw_Fill *fill,
		Enesim_Error **error)
{
	Enesim_Rasterizer_BiFigure *thiz;
	const Enesim_Renderer_State *cs = states[ENESIM_STATE_CURRENT];
	Enesim_Shape_Draw_Mode draw_mode;
	Enesim_Renderer *spaint, *fpaint;
	Enesim_Color color, scolor, fcolor;
	double sw;

	thiz = _bifigure_get(r);
	if (!thiz->under_figure && thiz->over_figure)
	{
		ENESIM_RENDERER_ERROR(r, error, "No figure to rasterize");
		return EINA_FALSE;
	}
	if (!thiz->over && !thiz->under)
	{
		ENESIM_RENDERER_ERROR(r, error, "No rasterizers to use");
		return EINA_FALSE;
	}

	enesim_renderer_color_get(r, &color);
	enesim_renderer_shape_draw_mode_get(r, &draw_mode);
	enesim_renderer_shape_stroke_weight_get(r, &sw);
	enesim_renderer_shape_stroke_color_get(r, &scolor);
	enesim_renderer_shape_stroke_renderer_get(r, &spaint);
	enesim_renderer_shape_fill_color_get(r, &fcolor);
	enesim_renderer_shape_fill_renderer_get(r, &fpaint);

	if (thiz->changed)
	{
		/* generate the 'over' figure */
		if (thiz->over_figure)
		{
			if (!thiz->over)
			{
				ENESIM_RENDERER_ERROR(r, error, "No over rasterizer");
				return EINA_FALSE;
			}
			enesim_rasterizer_figure_set(thiz->over, thiz->over_figure);
		}
		/* generate the 'under' figure */
		if (thiz->under_figure)
		{
			if (!thiz->under)
			{
				ENESIM_RENDERER_ERROR(r, error, "No under rasterizer");
				return EINA_FALSE;
			}
			enesim_rasterizer_figure_set(thiz->under, thiz->under_figure);
		}
		thiz->changed = EINA_FALSE;
	}

	/* setup the figures and get span funcs */
	if (!thiz->under)
	{
		enesim_renderer_origin_set(thiz->over, cs->ox, cs->oy);
		enesim_renderer_transformation_set(thiz->over, &cs->transformation);
		enesim_renderer_color_set(thiz->over, color);
		enesim_renderer_shape_fill_color_set(thiz->over, scolor);
		enesim_renderer_shape_fill_renderer_set(thiz->over, spaint);
		enesim_renderer_shape_draw_mode_set(thiz->over, ENESIM_SHAPE_DRAW_MODE_FILL);
		if (!enesim_renderer_setup(thiz->over, s, error))
			return EINA_FALSE;
		*fill = _over_figure_span;
	}
	else
	{
		if ( (sw <= 1) || (draw_mode == ENESIM_SHAPE_DRAW_MODE_FILL) )
		{
			enesim_renderer_origin_set(thiz->under, cs->ox, cs->oy);
			enesim_renderer_transformation_set(thiz->under, &cs->transformation);
			enesim_renderer_color_set(thiz->under, color);
			enesim_renderer_shape_draw_mode_set(thiz->under, draw_mode);
			enesim_renderer_shape_stroke_weight_set(thiz->under, 1);
			enesim_renderer_shape_stroke_color_set(thiz->under, scolor);
			enesim_renderer_shape_stroke_renderer_set(thiz->under, spaint);
			enesim_renderer_shape_fill_color_set(thiz->under, fcolor);
			enesim_renderer_shape_fill_renderer_set(thiz->under, fpaint);
			if (!enesim_renderer_setup(thiz->under, s, error))
				return EINA_FALSE;
			*fill = _under_figure_span;
		}
		else  // stroke_weight > 1 and draw_mode != FILL
		{
			if (thiz->over)
			{
				enesim_renderer_origin_set(thiz->over, cs->ox, cs->oy);
				enesim_renderer_transformation_set(thiz->over, &cs->transformation);
				enesim_renderer_color_set(thiz->over, color);
				enesim_renderer_shape_draw_mode_set(thiz->over, ENESIM_SHAPE_DRAW_MODE_FILL);
				enesim_renderer_shape_fill_color_set(thiz->over, scolor);
				enesim_renderer_shape_fill_renderer_set(thiz->over, spaint);
				if (draw_mode == ENESIM_SHAPE_DRAW_MODE_STROKE_FILL)
				{
					enesim_renderer_origin_set(thiz->under, cs->ox, cs->oy);
					enesim_renderer_transformation_set(thiz->under, &cs->transformation);
					enesim_renderer_color_set(thiz->under, color);
					enesim_renderer_shape_draw_mode_set(thiz->under, ENESIM_SHAPE_DRAW_MODE_STROKE_FILL);
					enesim_renderer_shape_fill_color_set(thiz->under, fcolor);
					enesim_renderer_shape_fill_renderer_set(thiz->under, fpaint);
					if (!enesim_renderer_setup(thiz->under, s, error))
						return EINA_FALSE;
				}
				if (!enesim_renderer_setup(thiz->over, s, error))
					return EINA_FALSE;

				if (draw_mode == ENESIM_SHAPE_DRAW_MODE_STROKE_FILL)
				{
					enesim_matrix_f16p16_matrix_to(&cs->transformation,
									&thiz->matrix);
					// need to define more cases, but for now...
					*fill = _bifig_stroke_paint_fill_paint_affine_simple;
					if (!spaint)
						*fill = _bifig_stroke_fill_paint_affine_simple;
					else if (!fpaint)
						*fill = _bifig_stroke_paint_fill_affine_simple;
				}
				else
				{
					*fill = _over_figure_span;
				}
			}
			else
			{
				enesim_renderer_origin_set(thiz->under, cs->ox, cs->oy);
				enesim_renderer_transformation_set(thiz->under, &cs->transformation);
				enesim_renderer_color_set(thiz->under, color);
				enesim_renderer_shape_draw_mode_set(thiz->under, draw_mode);
				enesim_renderer_shape_stroke_weight_set(thiz->under, 1);
				enesim_renderer_shape_stroke_color_set(thiz->under, scolor);
				enesim_renderer_shape_stroke_renderer_set(thiz->under, spaint);
				enesim_renderer_shape_fill_color_set(thiz->under, fcolor);
				enesim_renderer_shape_fill_renderer_set(thiz->under, fpaint);
				if (!enesim_renderer_setup(thiz->under, s, error))
					return EINA_FALSE;
				*fill = _under_figure_span;
			}
		}
	}
	/* do our own internal boundings */
	/* FIXME this tyy and byy in theory should be the same as the max/min of the under/over
	 * bounds, why do we need a double check on the span function? rendering only on the bounding
	 * box is something that is controlled on the path, i.e the renderer that *uses* this internal
	 * renderer */
	if (thiz->under && thiz->over)
	{
		double uxmin, xmin;
		double uymin, ymin;
		double uxmax, xmax;
		double uymax, ymax;

		enesim_figure_boundings(thiz->under_figure, &uxmin, &uymin, &uxmax, &uymax);
		enesim_figure_boundings(thiz->over_figure, &xmin, &ymin, &xmax, &ymax);

		if (uxmin < xmin)
			xmin = uxmin;
		if (uymin < ymin)
			ymin = uymin;
		if (uxmax > xmax)
			xmax = uxmax;
		if (uymax > ymax)
			ymax = uymax;

		thiz->tyy = eina_f16p16_double_from(ymin);
		thiz->byy = eina_f16p16_double_from(ymax);
	}

	return EINA_TRUE;
}

static void _bifigure_sw_cleanup(Enesim_Renderer *r, Enesim_Surface *s)
{
	Enesim_Rasterizer_BiFigure *thiz;

	thiz = _bifigure_get(r);
	/* FIXME We need to check that the over or under renderers were actually setup */
	if (thiz->over)
		enesim_renderer_cleanup(thiz->over, s);
	if (thiz->under)
		enesim_renderer_cleanup(thiz->under, s);
	thiz->changed = EINA_FALSE;
}

static void _bifigure_flags(Enesim_Renderer *r, Enesim_Renderer_Flag *flags)
{
	*flags = ENESIM_RENDERER_FLAG_AFFINE |
			ENESIM_RENDERER_FLAG_PROJECTIVE |
			ENESIM_RENDERER_FLAG_ARGB8888 |
			ENESIM_SHAPE_FLAG_FILL_RENDERER;
}

static Enesim_Rasterizer_Descriptor _descriptor = {
	/* .name = 		*/ _bifigure_name,
	/* .free = 		*/ _bifigure_free,
	/* .figure_set =	*/ _bifigure_figure_set,
	/* .sw_setup = 		*/ _bifigure_sw_setup,
	/* .sw_cleanup = 	*/ _bifigure_sw_cleanup,
};
/*============================================================================*
 *                                 Global                                     *
 *============================================================================*/
Enesim_Renderer * enesim_rasterizer_bifigure_new(void)
{
	Enesim_Rasterizer_BiFigure *thiz;
	Enesim_Renderer *r;

	thiz = calloc(1, sizeof(Enesim_Rasterizer_BiFigure));
	if (!thiz) return NULL;

	EINA_MAGIC_SET(thiz, ENESIM_RASTERIZER_BIFIGURE_MAGIC);
	r = enesim_rasterizer_new(&_descriptor, thiz);

	return r;
}

void enesim_rasterizer_bifigure_over_figure_set(Enesim_Renderer *r, const Enesim_Figure *figure)
{
	Enesim_Rasterizer_BiFigure *thiz;

	thiz = _bifigure_get(r);
	thiz->over_figure = (Enesim_Figure *)figure;
	if (figure && !thiz->over)
		thiz->over = enesim_rasterizer_basic_new();
	thiz->changed = EINA_TRUE;
}
/*============================================================================*
 *                                   API                                      *
 *============================================================================*/
