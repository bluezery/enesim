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
#include "Enesim.h"
#include "enesim_private.h"
#include "private/shape.h"
#include "libargb.h"

/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/
#define ENESIM_RENDERER_RECTANGLE_MAGIC_CHECK(d) \
	do {\
		if (!EINA_MAGIC_CHECK(d, ENESIM_RENDERER_RECTANGLE_MAGIC))\
			EINA_MAGIC_FAIL(d, ENESIM_RENDERER_RECTANGLE_MAGIC);\
	} while(0)

typedef struct _Enesim_Renderer_Rectangle_State
{
	double width;
	double height;
	double x;
	double y;
	struct {
		double radius;
		Eina_Bool tl : 1;
		Eina_Bool tr : 1;
		Eina_Bool bl : 1;
		Eina_Bool br : 1;
	} corner;
} Enesim_Renderer_Rectangle_State;

typedef struct _Enesim_Renderer_Rectangle_Sw
{
	Eina_F16p16 ww;
	Eina_F16p16 hh;
	Eina_F16p16 rr0, rr1;
	Eina_F16p16 stww;
	int stw; /* FIXME remove this */
} Enesim_Renderer_Rectangle_Sw;

/* FIXME struct in process, it should have the above structure twice, one for the inner
 * rectangle and one for the outter rectangle
 */
typedef struct _Enesim_Renderer_Rectangle_Sw_State
{
	Enesim_Renderer_Rectangle_Sw outter;
	Enesim_Renderer_Rectangle_Sw inner;
	Eina_F16p16 xx;
	Eina_F16p16 yy;
	Enesim_F16p16_Matrix matrix;
	/* the inner rectangle in case of rounded corners */
	Eina_F16p16 lxx0, rxx0;
	Eina_F16p16 tyy0, byy0;
	Eina_Bool do_inner : 1;
} Enesim_Renderer_Rectangle_Sw_State;

typedef struct _Enesim_Renderer_Rectangle
{
	EINA_MAGIC
	/* public properties */
	Enesim_Renderer_Rectangle_State current;
	Enesim_Renderer_Rectangle_State past;
	/* internal state */
	Eina_Bool changed : 1;
	/* for the case we use the path renderer */
	Enesim_Renderer *path;
	Enesim_Renderer_Rectangle_Sw_State sw_state;
} Enesim_Renderer_Rectangle;

/* we assume tyy and lxx are inside the top left corner */
/* FIXME we should replace all the ints, we are losing precision as this renderer
 * only handled integers coords
 */
static inline void _top_left(int sx, int sy, int stw,
		uint16_t ax, uint16_t ay,
		Eina_F16p16 lxx, Eina_F16p16 tyy, Eina_F16p16 rr0, Eina_F16p16 rr1,
		uint32_t cout[4], uint16_t *ca)
{
	if ((-lxx - tyy) >= rr0)
	{
		int rr = hypot(lxx, tyy);

		*ca = 0;
		if (rr < rr1)
		{
			*ca = 256;
			if (rr > rr0)
				*ca = 256 - ((rr - rr0) >> 8);
		}
	}

	if (sx < stw)
	{
		if (cout[1] != cout[3])
			cout[1] = argb8888_interp_256(ax, cout[3], cout[1]);
		cout[0] = cout[2] = cout[3] = cout[1];
	}
	if (sy < stw)
	{
		if (cout[2] != cout[3])
			cout[2] = argb8888_interp_256(ay, cout[3], cout[2]);
		cout[0] = cout[1] = cout[3] = cout[2];
	}
}

static inline void _bottom_left(int sx, int sy, int sh, int stw,
		uint16_t ax, uint16_t ay,
		Eina_F16p16 lxx, Eina_F16p16 byy, Eina_F16p16 rr0, Eina_F16p16 rr1,
		uint32_t cout[4], uint16_t *ca)
{
	if ((-lxx + byy) >= rr0)
	{
		int rr = hypot(lxx, byy);

		*ca = 0;
		if (rr < rr1)
		{
			*ca = 256;
			if (rr > rr0)
				*ca = 256 - ((rr - rr0) >> 8);
		}
	}

	if (sx < stw)
	{
		if (cout[1] != cout[3])
			cout[1] = argb8888_interp_256(ax, cout[3], cout[1]);
		cout[0] = cout[2] = cout[3] = cout[1];
	}
	if ((sy + 1 + stw) == sh)
	{
		if (cout[0] != cout[1])
			cout[0] = argb8888_interp_256(ay, cout[1], cout[0]);
		cout[1] = cout[2] = cout[3] = cout[0];
	}
}

static inline void _left_corners(int sx, int sy, int sh, int stw,
		uint16_t ax, uint16_t ay,
		Eina_Bool tl, Eina_Bool bl,
		Eina_F16p16 lxx, Eina_F16p16 tyy, Eina_F16p16 byy,
		Eina_F16p16 rr0, Eina_F16p16 rr1,
		uint32_t cout[4], uint16_t *ca)
{
	if (lxx < 0)
	{
		if (tl && (tyy < 0))
			_top_left(sx, sy, stw, ax, ay, lxx, tyy, rr0, rr1, cout, ca);
		if (bl && (byy > 0))
			_bottom_left(sx, sy, sh, stw, ax, ay, lxx, byy, rr0, rr1, cout, ca);
	}
}

static inline void _top_right(int sx, int sy, int sw, int stw,
		uint16_t ax, uint16_t ay,
		Eina_F16p16 rxx, Eina_F16p16 tyy,
		Eina_F16p16 rr0, Eina_F16p16 rr1,
		uint32_t cout[4], uint16_t *ca)
{
	if ((rxx - tyy) >= rr0)
	{
		int rr = hypot(rxx, tyy);

		*ca = 0;
		if (rr < rr1)
		{
			*ca = 256;
			if (rr > rr0)
				*ca = 256 - ((rr - rr0) >> 8);
		}
	}

	if ((sx + 1 + stw) == sw)
	{
		if (cout[0] != cout[2])
			cout[0] = argb8888_interp_256(ax, cout[2], cout[0]);
		cout[1] = cout[2] = cout[3] = cout[0];
	}
	if (sy < stw)
	{
		if (cout[2] != cout[3])
			cout[2] = argb8888_interp_256(ay, cout[3], cout[2]);
		cout[0] = cout[1] = cout[3] = cout[2];
	}
}

static inline void _bottom_right(int sx, int sy, int sw, int sh, int stw,
		uint16_t ax, uint16_t ay,
		Eina_F16p16 rxx, Eina_F16p16 byy,
		Eina_F16p16 rr0, Eina_F16p16 rr1,
		uint32_t cout[4], uint16_t *ca)
{
	if ((rxx + byy) >= rr0)
	{
		int rr = hypot(rxx, byy);

		*ca = 0;
		if (rr < rr1)
		{
			*ca = 256;
			if (rr > rr0)
				*ca = 256 - ((rr - rr0) >> 8);
		}
	}

	if ((sx + 1 + stw) == sw)
	{
		if (cout[0] != cout[2])
			cout[0] = argb8888_interp_256(ax, cout[2], cout[0]);
		cout[1] = cout[2] = cout[3] = cout[0];
	}
	if ((sy + 1 + stw) == sh)
	{
		if (cout[0] != cout[1])
			cout[0] = argb8888_interp_256(ay, cout[1], cout[0]);
		cout[1] = cout[2] = cout[3] = cout[0];
	}
}

static inline void _right_corners(int sx, int sy, int sw, int sh, int stw,
		uint16_t ax, uint16_t ay,
		Eina_Bool tr, Eina_Bool br,
		Eina_F16p16 rxx, Eina_F16p16 tyy, Eina_F16p16 byy,
		Eina_F16p16 rr0, Eina_F16p16 rr1,
		uint32_t cout[4], uint16_t *ca)
{
	if (rxx > 0)
	{
		if (tr && (tyy < 0))
			_top_right(sx, sy, sw, stw, ax, ay, rxx, tyy, rr0, rr1, cout, ca);
		if (br && (byy > 0))
			_bottom_right(sx, sy, sw, sh, stw, ax, ay, rxx, byy, rr0, rr1, cout, ca);
	}
}

static inline void _rectangle_corners(int sx, int sy, int sw, int sh, int stw, uint16_t ax, uint16_t ay,
		Eina_Bool tl, Eina_Bool tr, Eina_Bool br, Eina_Bool bl,
		Eina_F16p16 lxx, Eina_F16p16 rxx, Eina_F16p16 tyy, Eina_F16p16 byy,
		Eina_F16p16 rr0, Eina_F16p16 rr1,
		uint32_t cout[4], uint16_t *ca)
{
	_left_corners(sx, sy, sh, stw, ax, ay, tl, bl, lxx, tyy, byy, rr0, rr1, cout, ca);
	_right_corners(sx, sy, sw, sh, stw, ax, ay, tr, br, rxx, tyy, byy, rr0, rr1, cout, ca);
}

static inline Enesim_Renderer_Rectangle * _rectangle_get(Enesim_Renderer *r)
{
	Enesim_Renderer_Rectangle *thiz;

	thiz = enesim_renderer_shape_data_get(r);
	ENESIM_RENDERER_RECTANGLE_MAGIC_CHECK(thiz);

	return thiz;
}

static Eina_Bool _rectangle_use_path(Enesim_Matrix_Type geometry_type)
{
	if (geometry_type != ENESIM_MATRIX_IDENTITY)
		return EINA_TRUE;
	return EINA_FALSE;
}

static void _rectangle_path_setup(Enesim_Renderer_Rectangle *thiz,
		double x, double y, double w, double h, double r,
		const Enesim_Renderer_State *states[ENESIM_RENDERER_STATES],
		const Enesim_Renderer_Shape_State *sstates[ENESIM_RENDERER_STATES])
{
	const Enesim_Renderer_State *cs = states[ENESIM_STATE_CURRENT];
	const Enesim_Renderer_Shape_State *css = sstates[ENESIM_STATE_CURRENT];

	if (!thiz->path)
		thiz->path = enesim_renderer_path_new();
	/* FIXME the best approach would be to know *what* has changed
	 * because we only need to generate the vertices for x,y,w,h
	 * change
	 */
	/* FIXME or prev->geometry_transformation_type == IDENTITY && curr->geometry_transformation_type != IDENTITY */
	if (thiz->changed)
	{
		int count = 0;
		enesim_renderer_path_command_clear(thiz->path);
		/* FIXME for now handle the corners like this */
		if (thiz->current.corner.tl && r > 0.0)
		{
			enesim_renderer_path_move_to(thiz->path, x, y + r);
			enesim_renderer_path_quadratic_to(thiz->path, x, y, x + r, y);
			count++;
		}
		else
		{
			enesim_renderer_path_move_to(thiz->path, x, y);
			count++;
		}
		if (thiz->current.corner.tr && r > 0.0)
		{
			enesim_renderer_path_line_to(thiz->path, x + w - r, y);
			enesim_renderer_path_quadratic_to(thiz->path, x + w, y, x + w, y + r);
			count++;
		}
		else
		{
			enesim_renderer_path_line_to(thiz->path, x + w, y);
			count++;
		}
		if (thiz->current.corner.br && r > 0.0)
		{
			enesim_renderer_path_line_to(thiz->path, x + w, y + h - r);
			enesim_renderer_path_quadratic_to(thiz->path, x + w, y + h, x + w - r, y + h);
			count++;
		}
		else
		{
			enesim_renderer_path_line_to(thiz->path, x + w, y + h);
			count++;
		}
		if (thiz->current.corner.bl && r > 0.0)
		{
			enesim_renderer_path_line_to(thiz->path, x + r, y + h);
			enesim_renderer_path_quadratic_to(thiz->path, x, y + h, x, y + h - r);
			count++;
		}
		else
		{
			enesim_renderer_path_line_to(thiz->path, x, y + h);
			count++;
		}
		enesim_renderer_path_close(thiz->path, EINA_TRUE);
	}

	/* pass all the properties to the path */
	enesim_renderer_color_set(thiz->path, cs->color);
	enesim_renderer_origin_set(thiz->path, cs->ox, cs->oy);
	enesim_renderer_geometry_transformation_set(thiz->path, &cs->geometry_transformation);
	/* base properties */
	enesim_renderer_shape_fill_renderer_set(thiz->path, css->fill.r);
	enesim_renderer_shape_fill_color_set(thiz->path, css->fill.color);
	enesim_renderer_shape_stroke_renderer_set(thiz->path, css->stroke.r);
	enesim_renderer_shape_stroke_weight_set(thiz->path, css->stroke.weight);
	enesim_renderer_shape_stroke_color_set(thiz->path, css->stroke.color);
	enesim_renderer_shape_draw_mode_set(thiz->path, css->draw_mode);
}

static inline Enesim_Color _rectangle_sample(Eina_F16p16 xx, Eina_F16p16 yy,
			Enesim_Renderer_Rectangle_Sw *sws,
			Enesim_Renderer_Rectangle *thiz,
			Eina_F16p16 lxx, Eina_F16p16 rxx,
			Eina_F16p16 tyy, Eina_F16p16 byy,
			Enesim_Color ocolor, Enesim_Color icolor)
{
	Enesim_Color p3 = ocolor;
	Enesim_Color p2 = ocolor;
	Enesim_Color p1 = ocolor;
	Enesim_Color p0 = ocolor;
	Eina_F16p16 ww = sws->ww;
	Eina_F16p16 hh = sws->hh;
	int sx = xx >> 16;
	int sy = yy >> 16;
	int sw = ww >> 16;
	int sh = hh >> 16;
	uint16_t ca = 256;
	uint16_t ax = 1 + ((xx & 0xffff) >> 8);
	uint16_t ay = 1 + ((yy & 0xffff) >> 8);

	if ((ww - xx) < 65536)
		ax = 256 - ((ww - xx) >> 8);
	if ((hh - yy) < 65536)
		ay = 256 - ((hh - yy) >> 8);

	if ((sx > -1) & (sy > -1))
		p0 = icolor;
	if ((sy > -1) && ((xx + 65536) < ww))
		p1 = icolor;
	if ((yy + 65536) < hh)
	{
		if (sx > -1)
			p2 = icolor;
		if ((xx + 65536) < ww)
			p3 = icolor;
	}
	{
		Eina_Bool bl = thiz->current.corner.bl;
		Eina_Bool br = thiz->current.corner.br;
		Eina_Bool tl = thiz->current.corner.tl;
		Eina_Bool tr = thiz->current.corner.tr;
		Eina_F16p16 rr0 = sws->rr0;
		Eina_F16p16 rr1 = sws->rr1;
		int stw = sws->stw;
		uint32_t cout[4] = { p0, p1, p2, p3 };

		_rectangle_corners(sx, sy, sw, sh, stw, ax, ay, tl, tr, br, bl,
				lxx, rxx, tyy, byy, rr0, rr1, cout, &ca);


		p0 = cout[0];
		p1 = cout[1];
		p2 = cout[2];
		p3 = cout[3];
	}

	if (p0 != p1)
		p0 = argb8888_interp_256(ax, p1, p0);
	if (p2 != p3)
		p2 = argb8888_interp_256(ax, p3, p2);
	if (p0 != p2)
		p0 = argb8888_interp_256(ay, p2, p0);

	if (ca < 256)
		p0 = argb8888_interp_256(ca, p0, ocolor);

	return p0;
}

/*----------------------------------------------------------------------------*
 *                               Span functions                               *
 *----------------------------------------------------------------------------*/
/* Use the internal path for drawing */
static void _rectangle_path_span(Enesim_Renderer *r,
		const Enesim_Renderer_State *state,
		const Enesim_Renderer_Shape_State *sstate,
		int x, int y,
		unsigned int len, void *ddata)
{
	Enesim_Renderer_Rectangle *thiz;

	thiz = _rectangle_get(r);
	enesim_renderer_sw_draw(thiz->path, x, y, len, ddata);
}

/* stroke and/or fill with possibly a fill renderer */
static void _rounded_stroke_fill_paint_affine(Enesim_Renderer *r,
		const Enesim_Renderer_State *state,
		const Enesim_Renderer_Shape_State *sstate,
		int x, int y,
		unsigned int len, void *ddata)
{
	Enesim_Renderer_Rectangle *thiz = _rectangle_get(r);
	Enesim_Shape_Draw_Mode draw_mode;
	Eina_F16p16 ww0 = thiz->sw_state.outter.ww;
	Eina_F16p16 hh0 = thiz->sw_state.outter.hh;
	Eina_F16p16 xx0 = thiz->sw_state.xx;
	Eina_F16p16 yy0 = thiz->sw_state.yy;
	int axx = thiz->sw_state.matrix.xx;
	int ayx = thiz->sw_state.matrix.yx;
	int do_inner = thiz->sw_state.do_inner;
	Enesim_Color color;
	Enesim_Color ocolor;
	Enesim_Color icolor;
	int stww = thiz->sw_state.inner.stww;
	int iww = thiz->sw_state.inner.ww;
	int ihh = thiz->sw_state.inner.hh;
	int lxx0 = thiz->sw_state.lxx0, rxx0 = thiz->sw_state.rxx0;
	int tyy0 = thiz->sw_state.tyy0, byy0 = thiz->sw_state.byy0;
	Enesim_Renderer *fpaint;
	uint32_t *dst = ddata;
	unsigned int *d = dst, *e = d + len;
	int xx, yy;
	int fill_only = 0;

	enesim_renderer_shape_stroke_color_get(r, &ocolor);
	enesim_renderer_shape_fill_color_get(r, &icolor);
	enesim_renderer_shape_fill_renderer_get(r, &fpaint);
	enesim_renderer_shape_draw_mode_get(r, &draw_mode);

	enesim_renderer_color_get(r, &color);
	if (color != 0xffffffff)
	{
		ocolor = argb8888_mul4_sym(color, ocolor);
		icolor = argb8888_mul4_sym(color, icolor);
	}

	if (draw_mode == ENESIM_SHAPE_DRAW_MODE_STROKE)
	{
		icolor = 0;
		fpaint = NULL;
	}
	if (draw_mode == ENESIM_SHAPE_DRAW_MODE_FILL)
	{
		ocolor = icolor;
		fill_only = 1;
		do_inner = 0;
		if (fpaint)
		{
			enesim_renderer_sw_draw(fpaint, x, y, len, dst);
		}
	}
	if ((draw_mode == ENESIM_SHAPE_DRAW_MODE_STROKE_FILL) && do_inner && fpaint)
	{
		enesim_renderer_sw_draw(fpaint, x, y, len, dst);
	}

        enesim_renderer_affine_setup(r, x, y, &thiz->sw_state.matrix, &xx, &yy);
	xx = eina_f16p16_sub(xx, xx0);
	yy = eina_f16p16_sub(yy, yy0);

	while (d < e)
	{
		unsigned int q0 = 0;

		if (xx < ww0 && yy < hh0 && xx > -EINA_F16P16_ONE && yy > -EINA_F16P16_ONE)
		{
			Enesim_Color p0;
			int lxx = xx - lxx0, rxx = xx - rxx0;
			int tyy = yy - tyy0, byy = yy - byy0;
			int ixx = xx - stww, iyy = yy - stww;

			if (fill_only && fpaint)
			{
				ocolor = *d;
				if (icolor != 0xffffffff)
					ocolor = argb8888_mul4_sym(icolor, ocolor);
			}
			p0 = _rectangle_sample(xx, yy, &thiz->sw_state.outter, thiz,
					lxx, rxx, tyy, byy, 0, ocolor);

			if ( do_inner && (ixx > -EINA_F16P16_ONE) && (iyy > -EINA_F16P16_ONE) &&
				(ixx < iww) && (iyy < ihh) )
			{
				color = icolor;
				if (fpaint)
				{
					color = *d;
					if (icolor != 0xffffffff)
						color = argb8888_mul4_sym(icolor, color);
				}
				p0 = _rectangle_sample(ixx, iyy, &thiz->sw_state.inner, thiz,
						lxx, rxx, tyy, byy, p0, color);
			}
			q0 = p0;
		}
		*d++ = q0;
		xx += axx;
		yy += ayx;
	}
}

/* stroke with a renderer and possibly fill with color */
static void _rounded_stroke_paint_fill_affine(Enesim_Renderer *r,
		const Enesim_Renderer_State *state,
		const Enesim_Renderer_Shape_State *sstate,
		int x, int y,
		unsigned int len, void *ddata)
{
	Enesim_Renderer_Rectangle *thiz = _rectangle_get(r);
	Enesim_Shape_Draw_Mode draw_mode;
	Eina_F16p16 ww0 = thiz->sw_state.outter.ww;
	Eina_F16p16 hh0 = thiz->sw_state.outter.hh;
	Eina_F16p16 xx0 = thiz->sw_state.xx;
	Eina_F16p16 yy0 = thiz->sw_state.yy;
	int axx = thiz->sw_state.matrix.xx;
	int ayx = thiz->sw_state.matrix.yx;
	int do_inner = thiz->sw_state.do_inner;
	Enesim_Color color;
	Enesim_Color ocolor;
	Enesim_Color icolor;
	int stww = thiz->sw_state.inner.stww;
	int iww = thiz->sw_state.inner.ww;
	int ihh = thiz->sw_state.inner.hh;
	int lxx0 = thiz->sw_state.lxx0, rxx0 = thiz->sw_state.rxx0;
	int tyy0 = thiz->sw_state.tyy0, byy0 = thiz->sw_state.byy0;
	Enesim_Renderer *spaint;
	uint32_t *dst = ddata;
	unsigned int *d = dst, *e = d + len;
	int xx, yy;

	enesim_renderer_shape_stroke_color_get(r, &ocolor);
	enesim_renderer_shape_stroke_renderer_get(r, &spaint);
	enesim_renderer_shape_fill_color_get(r, &icolor);
	enesim_renderer_shape_draw_mode_get(r, &draw_mode);

	enesim_renderer_color_get(r, &color);
	if (color != 0xffffffff)
	{
		ocolor = argb8888_mul4_sym(color, ocolor);
		icolor = argb8888_mul4_sym(color, icolor);
	}

	enesim_renderer_sw_draw(spaint, x, y, len, dst);

	if (draw_mode == ENESIM_SHAPE_DRAW_MODE_STROKE)
		icolor = 0;

        enesim_renderer_affine_setup(r, x, y, &thiz->sw_state.matrix, &xx, &yy);
	xx = eina_f16p16_sub(xx, xx0);
	yy = eina_f16p16_sub(yy, yy0);

	while (d < e)
	{
		unsigned int q0 = 0;

		if (xx < ww0 && yy < hh0 && xx > -EINA_F16P16_ONE && yy > -EINA_F16P16_ONE)
		{
			Enesim_Color p0;
			int lxx = xx - lxx0, rxx = xx - rxx0;
			int tyy = yy - tyy0, byy = yy - byy0;
			int ixx = xx - stww, iyy = yy - stww;

			color = *d;
			if (ocolor != 0xffffffff)
				color = argb8888_mul4_sym(ocolor, color);

			p0 = _rectangle_sample(xx, yy, &thiz->sw_state.outter, thiz,
					lxx, rxx, tyy, byy, 0, color);
			if ( do_inner && (ixx > -EINA_F16P16_ONE) && (iyy > -EINA_F16P16_ONE) &&
				(ixx < iww) && (iyy < ihh) )
			{
				p0 = _rectangle_sample(ixx, iyy, &thiz->sw_state.inner, thiz,
						lxx, rxx, tyy, byy, p0, icolor);
			}
			q0 = p0;
		}
		*d++ = q0;
		xx += axx;
		yy += ayx;
	}
}

/* stroke and fill with renderers */
static void _rounded_stroke_paint_fill_paint_affine(Enesim_Renderer *r,
		const Enesim_Renderer_State *state,
		const Enesim_Renderer_Shape_State *sstate,
		int x, int y,
		unsigned int len, void *ddata)
{
	Enesim_Renderer_Rectangle *thiz = _rectangle_get(r);
	Enesim_Shape_Draw_Mode draw_mode;
	Eina_F16p16 ww0 = thiz->sw_state.outter.ww;
	Eina_F16p16 hh0 = thiz->sw_state.outter.hh;
	Eina_F16p16 xx0 = thiz->sw_state.xx;
	Eina_F16p16 yy0 = thiz->sw_state.yy;
	int axx = thiz->sw_state.matrix.xx;
	int ayx = thiz->sw_state.matrix.yx;
	Enesim_Color color;
	Enesim_Color ocolor;
	Enesim_Color icolor;
	int stww = thiz->sw_state.inner.stww;
	int iww = thiz->sw_state.inner.ww;
	int ihh = thiz->sw_state.inner.hh;
	int lxx0 = thiz->sw_state.lxx0, rxx0 = thiz->sw_state.rxx0;
	int tyy0 = thiz->sw_state.tyy0, byy0 = thiz->sw_state.byy0;
	Enesim_Renderer *fpaint, *spaint;
	uint32_t *dst = ddata;
	unsigned int *d = dst, *e = d + len;
	unsigned int *sbuf, *s;
	int xx, yy;

	enesim_renderer_shape_stroke_color_get(r, &ocolor);
	enesim_renderer_shape_stroke_renderer_get(r, &spaint);
	enesim_renderer_shape_fill_color_get(r, &icolor);
	enesim_renderer_shape_fill_renderer_get(r, &fpaint);
	enesim_renderer_shape_draw_mode_get(r, &draw_mode);

	enesim_renderer_color_get(r, &color);
	if (color != 0xffffffff)
	{
		ocolor = argb8888_mul4_sym(color, ocolor);
		icolor = argb8888_mul4_sym(color, icolor);
	}

	enesim_renderer_sw_draw(fpaint, x, y, len, dst);
	sbuf = alloca(len * sizeof(unsigned int));
	enesim_renderer_sw_draw(spaint, x, y, len, sbuf);
	s = sbuf;

        enesim_renderer_affine_setup(r, x, y, &thiz->sw_state.matrix, &xx, &yy);
	xx = eina_f16p16_sub(xx, xx0);
	yy = eina_f16p16_sub(yy, yy0);

	while (d < e)
	{
		unsigned int q0 = 0;

		if (xx < ww0 && yy < hh0 && xx > -EINA_F16P16_ONE && yy > -EINA_F16P16_ONE)
		{
			Enesim_Color p0;
			int lxx = xx - lxx0, rxx = xx - rxx0;
			int tyy = yy - tyy0, byy = yy - byy0;
			int ixx = xx - stww, iyy = yy - stww;

			color = *s;
			if (ocolor != 0xffffffff)
				color = argb8888_mul4_sym(ocolor, color);

			p0 = _rectangle_sample(xx, yy, &thiz->sw_state.outter, thiz,
					lxx, rxx, tyy, byy, 0, color);
			if ((ixx > -EINA_F16P16_ONE) && (iyy > -EINA_F16P16_ONE) &&
				(ixx < iww) && (iyy < ihh))
			{
				color = *d;
				if (icolor != 0xffffffff)
					color = argb8888_mul4_sym(icolor, color);
				p0 = _rectangle_sample(ixx, iyy, &thiz->sw_state.inner, thiz,
						lxx, rxx, tyy, byy, p0, color);
			}
			q0 = p0;
		}
		*d++ = q0;
		s++;
		xx += axx;
		yy += ayx;
	}
}

/* stroke and/or fill with possibly a fill renderer */
static void _rounded_stroke_fill_paint_proj(Enesim_Renderer *r,
		const Enesim_Renderer_State *state,
		const Enesim_Renderer_Shape_State *sstate,
		int x, int y,
		unsigned int len, void *ddata)
{
	Enesim_Renderer_Rectangle *thiz = _rectangle_get(r);
	Enesim_Shape_Draw_Mode draw_mode;
	Enesim_Color color;
	Enesim_Color ocolor;
	Enesim_Color icolor;
	Eina_F16p16 ww0 = thiz->sw_state.outter.ww;
	Eina_F16p16 hh0 = thiz->sw_state.outter.hh;
	Eina_F16p16 xx0 = thiz->sw_state.xx;
	Eina_F16p16 yy0 = thiz->sw_state.yy;
	int stww = thiz->sw_state.inner.stww;
	int iww = thiz->sw_state.inner.ww;
	int ihh = thiz->sw_state.inner.hh;
	int axx = thiz->sw_state.matrix.xx;
	int ayx = thiz->sw_state.matrix.yx;
	int azx = thiz->sw_state.matrix.zx;
	int do_inner = thiz->sw_state.do_inner;
	int lxx0 = thiz->sw_state.lxx0, rxx0 = thiz->sw_state.rxx0;
	int tyy0 = thiz->sw_state.tyy0, byy0 = thiz->sw_state.byy0;
	Enesim_Renderer *fpaint;
	uint32_t *dst = ddata;
	unsigned int *d = dst, *e = d + len;
	int xx, yy, zz;
	int fill_only = 0;

	enesim_renderer_shape_stroke_color_get(r, &ocolor);
	enesim_renderer_shape_fill_color_get(r, &icolor);
	enesim_renderer_shape_fill_renderer_get(r, &fpaint);
	enesim_renderer_shape_draw_mode_get(r, &draw_mode);

	enesim_renderer_color_get(r, &color);
	if (color != 0xffffffff)
	{
		ocolor = argb8888_mul4_sym(color, ocolor);
		icolor = argb8888_mul4_sym(color, icolor);
	}

	if (draw_mode == ENESIM_SHAPE_DRAW_MODE_STROKE)
	{
		icolor = 0;
		fpaint = NULL;
	}
	if (draw_mode == ENESIM_SHAPE_DRAW_MODE_FILL)
	{
		ocolor = icolor;
		fill_only = 1;
		do_inner = 0;
		if (fpaint)
		{
			enesim_renderer_sw_draw(fpaint, x, y, len, dst);
		}
	}
	if ((draw_mode == ENESIM_SHAPE_DRAW_MODE_STROKE_FILL) && do_inner && fpaint)
	{
		enesim_renderer_sw_draw(fpaint, x, y, len, dst);
	}
	enesim_renderer_projective_setup(r, x, y, &thiz->sw_state.matrix, &xx, &yy, &zz);
	xx = eina_f16p16_sub(xx, xx0);
	yy = eina_f16p16_sub(yy, yy0);

	while (d < e)
	{
		unsigned int q0 = 0;

		if (zz)
		{
			int sxx = (((long long int)xx) << 16) / zz;
			int syy = (((long long int)yy) << 16) / zz;

			if (sxx < ww0 && syy < hh0 && sxx > -EINA_F16P16_ONE && syy > -EINA_F16P16_ONE)
			{
				Enesim_Color p0;
				int lxx = sxx - lxx0, rxx = sxx - rxx0;
				int tyy = syy - tyy0, byy = syy - byy0;
				int ixx = sxx - stww, iyy = syy - stww;

				if (fill_only && fpaint)
				{
					ocolor = *d;
					if (icolor != 0xffffffff)
						ocolor = argb8888_mul4_sym(ocolor, icolor);
				}

				p0 = _rectangle_sample(xx, yy, &thiz->sw_state.outter, thiz,
						lxx, rxx, tyy, byy, 0, ocolor);
				if ( do_inner && (ixx > -EINA_F16P16_ONE) && (iyy > -EINA_F16P16_ONE) &&
					(ixx < iww) && (iyy < ihh) )
				{
					color = icolor;
					if (fpaint)
					{
						color = *d;
						if (icolor != 0xffffffff)
							color = argb8888_mul4_sym(icolor, color);
					}
					p0 = _rectangle_sample(ixx, iyy, &thiz->sw_state.inner, thiz,
							lxx, rxx, tyy, byy, p0, color);
				}
				q0 = p0;
			}
		}
		*d++ = q0;
		xx += axx;
		yy += ayx;
		zz += azx;
	}
}

/* stroke with a renderer and possibly fill with color */
static void _rounded_stroke_paint_fill_proj(Enesim_Renderer *r,
		const Enesim_Renderer_State *state,
		const Enesim_Renderer_Shape_State *sstate,
		int x, int y,
		unsigned int len, void *ddata)
{
	Enesim_Renderer_Rectangle *thiz = _rectangle_get(r);
	Enesim_Shape_Draw_Mode draw_mode;
	Enesim_Color color;
	Enesim_Color ocolor;
	Enesim_Color icolor;
	Eina_F16p16 ww0 = thiz->sw_state.outter.ww;
	Eina_F16p16 hh0 = thiz->sw_state.outter.hh;
	Eina_F16p16 xx0 = thiz->sw_state.xx;
	Eina_F16p16 yy0 = thiz->sw_state.yy;
	int stww = thiz->sw_state.inner.stww;
	int iww = thiz->sw_state.inner.ww;
	int ihh = thiz->sw_state.inner.hh;
	int axx = thiz->sw_state.matrix.xx;
	int ayx = thiz->sw_state.matrix.yx;
	int azx = thiz->sw_state.matrix.zx;
	int do_inner = thiz->sw_state.do_inner;
	int lxx0 = thiz->sw_state.lxx0, rxx0 = thiz->sw_state.rxx0;
	int tyy0 = thiz->sw_state.tyy0, byy0 = thiz->sw_state.byy0;
	Enesim_Renderer *spaint;
	uint32_t *dst = ddata;
	unsigned int *d = dst, *e = d + len;
	int xx, yy, zz;

	enesim_renderer_shape_stroke_color_get(r, &ocolor);
	enesim_renderer_shape_stroke_renderer_get(r, &spaint);
	enesim_renderer_shape_fill_color_get(r, &icolor);
	enesim_renderer_shape_draw_mode_get(r, &draw_mode);

	enesim_renderer_sw_draw(spaint, x, y, len, dst);

	if (draw_mode == ENESIM_SHAPE_DRAW_MODE_STROKE)
		icolor = 0;

	enesim_renderer_projective_setup(r, x, y, &thiz->sw_state.matrix, &xx, &yy, &zz);
	xx = eina_f16p16_sub(xx, xx0);
	yy = eina_f16p16_sub(yy, yy0);

	while (d < e)
	{
		unsigned int q0 = 0;

		if (zz)
		{
			int sxx = (((long long int)xx) << 16) / zz;
			int syy = (((long long int)yy) << 16) / zz;

			if (sxx < ww0 && syy < hh0 && sxx > -EINA_F16P16_ONE && syy > -EINA_F16P16_ONE)
			{
				Enesim_Color p0;
				int lxx = sxx - lxx0, rxx = sxx - rxx0;
				int tyy = syy - tyy0, byy = syy - byy0;
				int ixx = sxx - stww, iyy = syy - stww;

				color = *d;
				if (ocolor != 0xffffffff)
					color = argb8888_mul4_sym(ocolor, color);

				p0 = _rectangle_sample(xx, yy, &thiz->sw_state.outter, thiz,
						lxx, rxx, tyy, byy, 0, color);
				if ( do_inner && (ixx > -EINA_F16P16_ONE) && (iyy > -EINA_F16P16_ONE) &&
					(ixx < iww) && (iyy < ihh) )
				{
					p0 = _rectangle_sample(ixx, iyy, &thiz->sw_state.inner, thiz,
							lxx, rxx, tyy, byy, p0, icolor);
				}
				q0 = p0;
			}
		}
		*d++ = q0;
		xx += axx;
		yy += ayx;
		zz += azx;
	}
}

/* stroke and fill with renderers */
static void _rounded_stroke_paint_fill_paint_proj(Enesim_Renderer *r,
		const Enesim_Renderer_State *state,
		const Enesim_Renderer_Shape_State *sstate,
		int x, int y,
		unsigned int len, void *ddata)
{
	Enesim_Renderer_Rectangle *thiz = _rectangle_get(r);
	Enesim_Shape_Draw_Mode draw_mode;
	Enesim_Color color;
	Enesim_Color ocolor;
	Enesim_Color icolor;
	Eina_F16p16 ww0 = thiz->sw_state.outter.ww;
	Eina_F16p16 hh0 = thiz->sw_state.outter.hh;
	Eina_F16p16 xx0 = thiz->sw_state.xx;
	Eina_F16p16 yy0 = thiz->sw_state.yy;
	int stww = thiz->sw_state.inner.stww;
	int iww = thiz->sw_state.inner.ww;
	int ihh = thiz->sw_state.inner.hh;
	int axx = thiz->sw_state.matrix.xx;
	int ayx = thiz->sw_state.matrix.yx;
	int azx = thiz->sw_state.matrix.zx;
	int lxx0 = thiz->sw_state.lxx0, rxx0 = thiz->sw_state.rxx0;
	int tyy0 = thiz->sw_state.tyy0, byy0 = thiz->sw_state.byy0;
	Enesim_Renderer *fpaint, *spaint;
	uint32_t *dst = ddata;
	unsigned int *d = dst, *e = d + len;
	unsigned int *sbuf, *s;
	int xx, yy, zz;

	enesim_renderer_shape_stroke_color_get(r, &ocolor);
	enesim_renderer_shape_stroke_renderer_get(r, &spaint);
	enesim_renderer_shape_fill_color_get(r, &icolor);
	enesim_renderer_shape_fill_renderer_get(r, &fpaint);
	enesim_renderer_shape_draw_mode_get(r, &draw_mode);
	enesim_renderer_color_get(r, &color);
	if (color != 0xffffffff)
	{
		ocolor = argb8888_mul4_sym(color, ocolor);
		icolor = argb8888_mul4_sym(color, icolor);
	}

	enesim_renderer_sw_draw(fpaint, x, y, len, dst);
	sbuf = alloca(len * sizeof(unsigned int));
	enesim_renderer_sw_draw(spaint, x, y, len, sbuf);
	s = sbuf;

	enesim_renderer_projective_setup(r, x, y, &thiz->sw_state.matrix, &xx, &yy, &zz);
	xx = eina_f16p16_sub(xx, xx0);
	yy = eina_f16p16_sub(yy, yy0);

	while (d < e)
	{
		unsigned int q0 = 0;

		if (zz)
		{
			int sxx = (((long long int)xx) << 16) / zz;
			int syy = (((long long int)yy) << 16) / zz;

			if (sxx < ww0 && syy < hh0 && sxx > -EINA_F16P16_ONE && syy > -EINA_F16P16_ONE)
			{
				Enesim_Color p0;
				int lxx = sxx - lxx0, rxx = sxx - rxx0;
				int tyy = syy - tyy0, byy = syy - byy0;
				int ixx = sxx - stww, iyy = syy - stww;

				color = *s;
				if (ocolor != 0xffffffff)
					color = argb8888_mul4_sym(ocolor, color);

				p0 = _rectangle_sample(xx, yy, &thiz->sw_state.outter, thiz,
						lxx, rxx, tyy, byy, 0, color);

				if ((ixx > -EINA_F16P16_ONE) && (iyy > -EINA_F16P16_ONE) &&
					(ixx < iww) && (iyy < ihh))
				{
					color = *d;
					if (icolor != 0xffffffff)
						color = argb8888_mul4_sym(icolor, color);

					p0 = _rectangle_sample(ixx, iyy, &thiz->sw_state.inner, thiz,
							lxx, rxx, tyy, byy, p0, color);
				}
				q0 = p0;
			}
		}
		*d++ = q0;
		s++;
		xx += axx;
		yy += ayx;
		zz += azx;
	}
}
/*----------------------------------------------------------------------------*
 *                      The Enesim's renderer interface                       *
 *----------------------------------------------------------------------------*/
static const char * _rectangle_name(Enesim_Renderer *r)
{
	return "rectangle";
}

static Eina_Bool _rectangle_state_setup(Enesim_Renderer *r,
		const Enesim_Renderer_State *states[ENESIM_RENDERER_STATES],
		const Enesim_Renderer_Shape_State *sstates[ENESIM_RENDERER_STATES],
		Enesim_Surface *s,
		Enesim_Renderer_Shape_Sw_Draw *draw, Enesim_Error **error)
{
	Enesim_Renderer_Rectangle *thiz;
	Enesim_Shape_Draw_Mode draw_mode;
	Enesim_Renderer *spaint;
	const Enesim_Renderer_State *cs = states[ENESIM_STATE_CURRENT];
	const Enesim_Renderer_Shape_State *css = sstates[ENESIM_STATE_CURRENT];
	double rad;
	double x;
	double y;
	double w;
	double h;

	thiz = _rectangle_get(r);
	if (!thiz)
	{
		return EINA_FALSE;
	}

	w = thiz->current.width * cs->sx;
	h = thiz->current.height * cs->sy;
	if ((w < 1) || (h < 1))
	{
		ENESIM_RENDERER_ERROR(r, error, "Invalid size %g %g", w, h);
		return EINA_FALSE;
	}

	x = thiz->current.x * cs->sx;
	y = thiz->current.y * cs->sy;

	rad = thiz->current.corner.radius;
	if (rad > (w / 2.0))
		rad = w / 2.0;
	if (rad > (h / 2.0))
		rad = h / 2.0;

	/* check if we should use the path approach */
	if (_rectangle_use_path(cs->geometry_transformation_type))
	{
		double sw;

		sw = css->stroke.weight;
		/* in case of a stroked rectangle we need to convert the location of the stroke to center */
		if (css->draw_mode & ENESIM_SHAPE_DRAW_MODE_STROKE)
		{
			switch (css->stroke.location)
			{
				case ENESIM_SHAPE_STROKE_OUTSIDE:
				x -= sw / 2.0;
				y -= sw / 2.0;
				w += sw;
				h += sw;
				break;

				case ENESIM_SHAPE_STROKE_INSIDE:
				x += sw / 2.0;
				y += sw / 2.0;
				w -= sw;
				h -= sw;
				break;

				default:
				break;
			}
		}
		_rectangle_path_setup(thiz, x, y, w, h, rad, states, sstates);
		if (!enesim_renderer_setup(thiz->path, s, error))
		{
			return EINA_FALSE;
		}
		*draw = _rectangle_path_span;

		return EINA_TRUE;
	}
	/* do our own setup */
	else
	{
		double sw = css->stroke.weight;

		if (css->draw_mode & ENESIM_SHAPE_DRAW_MODE_STROKE)
		{
			/* handle the different stroke locations */
			switch (css->stroke.location)
			{
				case ENESIM_SHAPE_STROKE_CENTER:
				x -= sw / 2.0;
				y -= sw / 2.0;
				w += sw;
				h += sw;
				break;

				case ENESIM_SHAPE_STROKE_OUTSIDE:
				x -= sw;
				y -= sw;
				w += sw * 2.0;
				h += sw * 2.0;
				break;

				default:
				break;
			}
		}

		/* the code from here is only meaningful for our own span functions */
		thiz->sw_state.xx = eina_f16p16_double_from(x);
		thiz->sw_state.yy = eina_f16p16_double_from(y);

		/* outter state */
		thiz->sw_state.outter.ww = eina_f16p16_double_from(w);
		thiz->sw_state.outter.hh = eina_f16p16_double_from(h);
		thiz->sw_state.outter.rr0 = eina_f16p16_double_from(rad);
		thiz->sw_state.outter.rr1 = thiz->sw_state.outter.rr0 + 65536;
		thiz->sw_state.inner.stww = 0;
		/* FIXME we should not use the stw */
		thiz->sw_state.inner.stw = 0;
		thiz->sw_state.lxx0 = thiz->sw_state.outter.rr0;
		thiz->sw_state.tyy0 = thiz->sw_state.outter.rr0;
		thiz->sw_state.rxx0 = eina_f16p16_double_from(w - rad - 1);
		thiz->sw_state.byy0 = eina_f16p16_double_from(h - rad - 1);

		thiz->sw_state.do_inner = 1;
		if ((sw >= w / 2.0) || (sw >= h / 2.0))
		{
			sw = 0;
			thiz->sw_state.do_inner = 0;
		}
		rad = rad - sw;
		if (rad < 0.0039)
			rad = 0;

		/* inner state */
		thiz->sw_state.inner.rr0 = eina_f16p16_double_from(rad);
		thiz->sw_state.inner.rr1 = thiz->sw_state.inner.rr0 + 65536;
		thiz->sw_state.inner.stww = eina_f16p16_double_from(sw);
		/* FIXME we should not use the stw */
		thiz->sw_state.inner.stw = eina_f16p16_int_to(thiz->sw_state.inner.stww);
		thiz->sw_state.inner.ww = thiz->sw_state.outter.ww - (2 * thiz->sw_state.inner.stww);
		thiz->sw_state.inner.hh = thiz->sw_state.outter.hh - (2 * thiz->sw_state.inner.stww);

		if (!enesim_renderer_shape_setup(r, states, s, error))
		{
			ENESIM_RENDERER_ERROR(r, error, "Shape cannot setup");
			return EINA_FALSE;
		}

		enesim_matrix_f16p16_matrix_to(&cs->transformation,
				&thiz->sw_state.matrix);
		enesim_renderer_shape_draw_mode_get(r, &draw_mode);
		enesim_renderer_shape_stroke_renderer_get(r, &spaint);

		if (cs->transformation_type == ENESIM_MATRIX_AFFINE ||
			 cs->transformation_type == ENESIM_MATRIX_IDENTITY)
		{
			*draw = _rounded_stroke_fill_paint_affine;
			if ((sw != 0.0) && spaint && (draw_mode & ENESIM_SHAPE_DRAW_MODE_STROKE))
			{
				Enesim_Renderer *fpaint;

				*draw = _rounded_stroke_paint_fill_affine;
				enesim_renderer_shape_fill_renderer_get(r, &fpaint);
				if (fpaint && thiz->sw_state.do_inner &&
						(draw_mode & ENESIM_SHAPE_DRAW_MODE_FILL))
					*draw = _rounded_stroke_paint_fill_paint_affine;
			}
		}
		else
		{
			*draw = _rounded_stroke_fill_paint_proj;
			if ((sw != 0.0) && spaint && (draw_mode & ENESIM_SHAPE_DRAW_MODE_STROKE))
			{
				Enesim_Renderer *fpaint;

				*draw = _rounded_stroke_paint_fill_proj;
				enesim_renderer_shape_fill_renderer_get(r, &fpaint);
				if (fpaint && thiz->sw_state.do_inner &&
						(draw_mode & ENESIM_SHAPE_DRAW_MODE_FILL))
					*draw = _rounded_stroke_paint_fill_paint_proj;
			}
		}
		return EINA_TRUE;
	}
}

static void _rectangle_state_cleanup(Enesim_Renderer *r, Enesim_Surface *s)
{
	Enesim_Renderer_Rectangle *thiz;

	thiz = _rectangle_get(r);

	enesim_renderer_shape_cleanup(r, s);
	thiz->past = thiz->current;
	thiz->changed = EINA_FALSE;
}

static void _rectangle_flags(Enesim_Renderer *r, const Enesim_Renderer_State *state,
		Enesim_Renderer_Flag *flags)
{
	*flags = ENESIM_RENDERER_FLAG_TRANSLATE |
			ENESIM_RENDERER_FLAG_SCALE |
			ENESIM_RENDERER_FLAG_AFFINE |
			ENESIM_RENDERER_FLAG_PROJECTIVE |
			ENESIM_RENDERER_FLAG_GEOMETRY |
			ENESIM_RENDERER_FLAG_ARGB8888;
}

static void _rectangle_hints(Enesim_Renderer *r, const Enesim_Renderer_State *state,
		Enesim_Renderer_Hint *hints)
{
	*hints = ENESIM_RENDERER_HINT_COLORIZE;
}

static void _rectangle_feature_get(Enesim_Renderer *r, Enesim_Shape_Feature *features)
{
	*features = ENESIM_SHAPE_FLAG_FILL_RENDERER | ENESIM_SHAPE_FLAG_STROKE_RENDERER;
}

static void _rectangle_free(Enesim_Renderer *r)
{
	Enesim_Renderer_Rectangle *thiz;

	thiz = _rectangle_get(r);
	if (thiz->path)
		enesim_renderer_unref(thiz->path);
	free(thiz);
}

static void _rectangle_boundings(Enesim_Renderer *r,
		const Enesim_Renderer_State *states[ENESIM_RENDERER_STATES],
		const Enesim_Renderer_Shape_State *sstates[ENESIM_RENDERER_STATES],
		Enesim_Rectangle *boundings)
{
	Enesim_Renderer_Rectangle *thiz;
	const Enesim_Renderer_State *cs = states[ENESIM_STATE_CURRENT];
	const Enesim_Renderer_Shape_State *css = sstates[ENESIM_STATE_CURRENT];
	double x, y, w, h;

	thiz = _rectangle_get(r);

	/* first scale */
	x = thiz->current.x * cs->sx;
	y = thiz->current.y * cs->sy;
	w = thiz->current.width * cs->sx;
	h = thiz->current.height * cs->sy;
	/* for the stroke location get the real width */
	if (css->draw_mode & ENESIM_SHAPE_DRAW_MODE_STROKE)
	{
		double sw = css->stroke.weight;
		switch (css->stroke.location)
		{
			case ENESIM_SHAPE_STROKE_CENTER:
			x -= sw / 2.0;
			y -= sw / 2.0;
			w += sw;
			h += sw;
			break;

			case ENESIM_SHAPE_STROKE_OUTSIDE:
			x -= sw;
			y -= sw;
			w += sw * 2.0;
			h += sw * 2.0;
			break;

			default:
			break;
		}
	}

	boundings->x = x;
	boundings->y = y;
	boundings->w = w;
	boundings->h = h;

	/* translate by the origin */
	boundings->x += cs->ox;
	boundings->y += cs->oy;
	/* apply the geometry transformation */
	if (cs->geometry_transformation_type != ENESIM_MATRIX_IDENTITY)
	{
		Enesim_Quad q;

		enesim_matrix_rectangle_transform(&cs->geometry_transformation, boundings, &q);
		enesim_quad_rectangle_to(&q, boundings);
	}
}

static void _rectangle_destination_boundings(Enesim_Renderer *r,
		const Enesim_Renderer_State *states[ENESIM_RENDERER_STATES],
		const Enesim_Renderer_Shape_State *sstates[ENESIM_RENDERER_STATES],
		Eina_Rectangle *boundings)
{
	Enesim_Rectangle oboundings;
	const Enesim_Renderer_State *cs = states[ENESIM_STATE_CURRENT];

	_rectangle_boundings(r, states, sstates, &oboundings);
	/* apply the inverse matrix */
	if (cs->transformation_type != ENESIM_MATRIX_IDENTITY)
	{
		Enesim_Quad q;
		Enesim_Matrix m;

		enesim_matrix_inverse(&cs->transformation, &m);
		enesim_matrix_rectangle_transform(&m, &oboundings, &q);
		enesim_quad_rectangle_to(&q, &oboundings);
		/* fix the antialias scaling */
		boundings->x -= m.xx;
		boundings->y -= m.yy;
		boundings->w += m.xx;
		boundings->h += m.yy;
	}
	boundings->x = floor(oboundings.x);
	boundings->y = floor(oboundings.y);
	boundings->w = ceil(oboundings.x - boundings->x + oboundings.w) + 1;
	boundings->h = ceil(oboundings.y - boundings->y + oboundings.h) + 1;
}

static Eina_Bool _rectangle_has_changed(Enesim_Renderer *r,
		const Enesim_Renderer_State *states[ENESIM_RENDERER_STATES])
{
	Enesim_Renderer_Rectangle *thiz;

	thiz = _rectangle_get(r);

	if (!thiz->changed) return EINA_FALSE;

	/* the width */
	if (thiz->current.width != thiz->past.width)
		return EINA_TRUE;
	/* the height */
	if (thiz->current.height != thiz->past.height)
		return EINA_TRUE;
	/* the x */
	if (thiz->current.x != thiz->past.x)
		return EINA_TRUE;
	/* the y */
	if (thiz->current.y != thiz->past.y)
		return EINA_TRUE;
	/* the corners */
	if ((thiz->current.corner.tl != thiz->past.corner.tl) || (thiz->current.corner.tr != thiz->past.corner.tr) ||
	     (thiz->current.corner.bl != thiz->past.corner.bl) || (thiz->current.corner.br != thiz->past.corner.br))
		return EINA_TRUE;
	/* the corner radius */
	if (thiz->current.corner.radius != thiz->past.corner.radius)
		return EINA_TRUE;

	return EINA_FALSE;
}

#if BUILD_OPENGL
/* TODO instead of trying to do our own geometry shader it might be better
 * to use the path renderer and only create a shader there, this will simplify the code
 * given that at the end we'll generate vertices too
 */
static Eina_Bool _rectangle_opengl_setup(Enesim_Renderer *r,
		const Enesim_Renderer_State *states[ENESIM_RENDERER_STATES],
		const Enesim_Renderer_Shape_State *sstates[ENESIM_RENDERER_STATES],
		Enesim_Surface *s,
		int *num_shaders,
		Enesim_Renderer_OpenGL_Shader **shaders,
		Enesim_Error **error)
{
	Enesim_Renderer_Rectangle *thiz;
	Enesim_Renderer_OpenGL_Shader *shader;

 	thiz = _rectangle_get(r);

	/* FIXME in order to generate the stroke we might need to call this twice
	 * one for the outter rectangle and one for the inner
	 */
	*num_shaders = 2;
	*shaders = shader = calloc(*num_shaders, sizeof(Enesim_Renderer_OpenGL_Shader));
	shader->type = ENESIM_SHADER_GEOMETRY;
	shader->name = "rectangle";
		//"#version 150\n"
	shader->source = 
		"#version 120\n"
		"#extension GL_EXT_geometry_shader4 : enable\n"
	#include "enesim_renderer_rectangle.glsl"
	shader->size = strlen(shader->source);

	/* FIXME for now we should use the background renderer for the color */
	/* if we have a renderer set use that one but render into another texture
	 * if it has some geometric shader
	 */
	shader++;
	shader->type = ENESIM_SHADER_FRAGMENT;
	shader->name = "stripes";
	shader->source = 
	#include "enesim_renderer_stripes.glsl"
	shader->size = strlen(shader->source);

	return EINA_TRUE;
}

static Eina_Bool _rectangle_opengl_shader_setup(Enesim_Renderer *r, Enesim_Surface *s)
{
	Enesim_Renderer_Rectangle *thiz;
	Enesim_Renderer_OpenGL_Data *rdata;
	int width;
	int height;
	int x;
	int y;

 	thiz = _rectangle_get(r);
	rdata = enesim_renderer_backend_data_get(r, ENESIM_BACKEND_OPENGL);

	x = glGetUniformLocationARB(rdata->program, "rectangle_x");
	y = glGetUniformLocationARB(rdata->program, "rectangle_y");
	width = glGetUniformLocationARB(rdata->program, "rectangle_width");
	height = glGetUniformLocationARB(rdata->program, "rectangle_height");

	glUniform1f(x, thiz->current.x);
	glUniform1f(y, thiz->current.y);
	glUniform1f(width, thiz->current.width);
	glUniform1f(height, thiz->current.height);

	/* FIXME set the background like this for now */
	{
		int odd_color;
		int even_color;
		int odd_thickness;
		int even_thickness;

		even_color = glGetUniformLocationARB(rdata->program, "stripes_even_color");
		odd_color = glGetUniformLocationARB(rdata->program, "stripes_odd_color");
		even_thickness = glGetUniformLocationARB(rdata->program, "stripes_even_thickness");
		odd_thickness = glGetUniformLocationARB(rdata->program, "stripes_odd_thickness");

		glUniform4fARB(even_color, 1.0, 0.0, 0.0, 1.0);
		glUniform4fARB(odd_color, 0.0, 0.0, 1.0, 1.0);
		glUniform1i(even_thickness, 2.0);
		glUniform1i(odd_thickness, 5.0);
	}

	return EINA_TRUE;
}

static void _rectangle_opengl_cleanup(Enesim_Renderer *r, Enesim_Surface *s)
{
	Enesim_Renderer_Rectangle *thiz;

 	thiz = _rectangle_get(r);
}
#endif

static Enesim_Renderer_Shape_Descriptor _rectangle_descriptor = {
	/* .name = 			*/ _rectangle_name,
	/* .free = 			*/ _rectangle_free,
	/* .boundings = 		*/ _rectangle_boundings,
	/* .destination_boundings = 	*/ _rectangle_destination_boundings,
	/* .flags = 			*/ _rectangle_flags,
	/* .hints_get = 			*/ _rectangle_hints,
	/* .is_inside = 		*/ NULL,
	/* .damage = 			*/ NULL,
	/* .has_changed = 		*/ _rectangle_has_changed,
	/* .feature_get =		*/ _rectangle_feature_get,
	/* .sw_setup = 			*/ _rectangle_state_setup,
	/* .sw_cleanup = 		*/ _rectangle_state_cleanup,
	/* .opencl_setup =		*/ NULL,
	/* .opencl_kernel_setup =	*/ NULL,
	/* .opencl_cleanup =		*/ NULL,
#if BUILD_OPENGL
	/* .opengl_setup =          	*/ _rectangle_opengl_setup,
	/* .opengl_shader_setup =   	*/ _rectangle_opengl_shader_setup,
	/* .opengl_cleanup =        	*/ _rectangle_opengl_cleanup
#else
	/* .opengl_setup =          	*/ NULL,
	/* .opengl_shader_setup =   	*/ NULL,
	/* .opengl_cleanup =        	*/ NULL
#endif
};
/*============================================================================*
 *                                 Global                                     *
 *============================================================================*/
/*============================================================================*
 *                                   API                                      *
 *============================================================================*/
/**
 * Creates a new rectangle renderer
 * @return The new renderer
 */
EAPI Enesim_Renderer * enesim_renderer_rectangle_new(void)
{
	Enesim_Renderer *r;
	Enesim_Renderer_Rectangle *thiz;

	thiz = calloc(1, sizeof(Enesim_Renderer_Rectangle));
	if (!thiz) return NULL;
	EINA_MAGIC_SET(thiz, ENESIM_RENDERER_RECTANGLE_MAGIC);
	r = enesim_renderer_shape_new(&_rectangle_descriptor, thiz);
	/* to maintain compatibility */
	enesim_renderer_shape_stroke_location_set(r, ENESIM_SHAPE_STROKE_INSIDE);
	return r;
}
/**
 * Sets the width of the rectangle
 * @param[in] r The rectangle renderer
 * @param[in] width The rectangle width
 */
EAPI void enesim_renderer_rectangle_width_set(Enesim_Renderer *r, double width)
{
	Enesim_Renderer_Rectangle *thiz;

	thiz = _rectangle_get(r);
	thiz->current.width = width;
	thiz->changed = EINA_TRUE;
}
/**
 * Gets the width of the rectangle
 * @param[in] r The rectangle renderer
 * @param[out] w The rectangle width
 */
EAPI void enesim_renderer_rectangle_width_get(Enesim_Renderer *r, double *width)
{
	Enesim_Renderer_Rectangle *thiz;

	thiz = _rectangle_get(r);
	if (width) *width = thiz->current.width;
}
/**
 * Sets the height of the rectangle
 * @param[in] r The rectangle renderer
 * @param[in] height The rectangle height
 */
EAPI void enesim_renderer_rectangle_height_set(Enesim_Renderer *r, double height)
{
	Enesim_Renderer_Rectangle *thiz;

	thiz = _rectangle_get(r);
	thiz->current.height = height;
	thiz->changed = EINA_TRUE;
}
/**
 * Gets the height of the rectangle
 * @param[in] r The rectangle renderer
 * @param[out] height The rectangle height
 */
EAPI void enesim_renderer_rectangle_height_get(Enesim_Renderer *r, double *height)
{
	Enesim_Renderer_Rectangle *thiz;

	thiz = _rectangle_get(r);
	if (height) *height = thiz->current.height;
}

/**
 * Sets the x of the rectangle
 * @param[in] r The rectangle renderer
 * @param[in] x The rectangle x coordinate
 */
EAPI void enesim_renderer_rectangle_x_set(Enesim_Renderer *r, double x)
{
	Enesim_Renderer_Rectangle *thiz;

	thiz = _rectangle_get(r);
	thiz->current.x = x;
	thiz->changed = EINA_TRUE;
}
/**
 * Gets the x of the rectangle
 * @param[in] r The rectangle renderer
 * @param[out] w The rectangle x coordinate
 */
EAPI void enesim_renderer_rectangle_x_get(Enesim_Renderer *r, double *x)
{
	Enesim_Renderer_Rectangle *thiz;

	thiz = _rectangle_get(r);
	if (x) *x = thiz->current.x;
}
/**
 * Sets the y of the rectangle
 * @param[in] r The rectangle renderer
 * @param[in] y The rectangle y coordinate
 */
EAPI void enesim_renderer_rectangle_y_set(Enesim_Renderer *r, double y)
{
	Enesim_Renderer_Rectangle *thiz;

	thiz = _rectangle_get(r);
	thiz->current.y = y;
	thiz->changed = EINA_TRUE;
}
/**
 * Gets the y of the rectangle
 * @param[in] r The rectangle renderer
 * @param[out] y The rectangle y coordinate
 */
EAPI void enesim_renderer_rectangle_y_get(Enesim_Renderer *r, double *y)
{
	Enesim_Renderer_Rectangle *thiz;

	thiz = _rectangle_get(r);
	if (y) *y = thiz->current.y;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_rectangle_position_set(Enesim_Renderer *r, double x, double y)
{
	Enesim_Renderer_Rectangle *thiz;
	thiz = _rectangle_get(r);
	thiz->current.x = x;
	thiz->current.y = y;
	thiz->changed = EINA_TRUE;
}
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_rectangle_position_get(Enesim_Renderer *r, double *x, double *y)
{
	Enesim_Renderer_Rectangle *thiz;

	thiz = _rectangle_get(r);
	if (x) *x = thiz->current.x;
	if (y) *y = thiz->current.y;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_rectangle_size_set(Enesim_Renderer *r, double width, double height)
{
	Enesim_Renderer_Rectangle *thiz;
	thiz = _rectangle_get(r);
	thiz->current.width = width;
	thiz->current.height = height;
	thiz->changed = EINA_TRUE;
}
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_rectangle_size_get(Enesim_Renderer *r, double *width, double *height)
{
	Enesim_Renderer_Rectangle *thiz;

	thiz = _rectangle_get(r);
	if (width) *width = thiz->current.width;
	if (height) *height = thiz->current.height;
}
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_rectangle_corner_radius_set(Enesim_Renderer *r, double radius)
{
	Enesim_Renderer_Rectangle *thiz;
	thiz = _rectangle_get(r);
	if (!thiz) return;
	if (radius < 0)
		radius = 0;
	if (thiz->current.corner.radius == radius)
		return;
	thiz->current.corner.radius = radius;
	thiz->changed = EINA_TRUE;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_rectangle_top_left_corner_set(Enesim_Renderer *r, Eina_Bool rounded)
{
	Enesim_Renderer_Rectangle *thiz;

	thiz = _rectangle_get(r);
	thiz->current.corner.tl = rounded;
	thiz->changed = EINA_TRUE;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_rectangle_top_right_corner_set(Enesim_Renderer *r, Eina_Bool rounded)
{
	Enesim_Renderer_Rectangle *thiz;

	thiz = _rectangle_get(r);
	thiz->current.corner.tr = rounded;
	thiz->changed = EINA_TRUE;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_rectangle_bottom_left_corner_set(Enesim_Renderer *r, Eina_Bool rounded)
{
	Enesim_Renderer_Rectangle *thiz;

	thiz = _rectangle_get(r);
	thiz->current.corner.bl = rounded;
	thiz->changed = EINA_TRUE;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_rectangle_bottom_right_corner_set(Enesim_Renderer *r, Eina_Bool rounded)
{
	Enesim_Renderer_Rectangle *thiz;

	thiz = _rectangle_get(r);
	thiz->current.corner.br = rounded;
	thiz->changed = EINA_TRUE;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_rectangle_corners_set(Enesim_Renderer *r,
		Eina_Bool tl, Eina_Bool tr, Eina_Bool bl, Eina_Bool br)
{
	Enesim_Renderer_Rectangle *thiz;

	thiz = _rectangle_get(r);
	if (!thiz) return;

	if ((thiz->current.corner.tl == tl) && (thiz->current.corner.tr == tr) &&
	     (thiz->current.corner.bl == bl) && (thiz->current.corner.br == br))
		return;
	thiz->current.corner.tl = tl;  thiz->current.corner.tr = tr;
	thiz->current.corner.bl = bl;  thiz->current.corner.br = br;
	thiz->changed = EINA_TRUE;
}
