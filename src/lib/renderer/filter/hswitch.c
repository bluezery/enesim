/* ENESIM - Direct Rendering Library
 * Copyright (C) 2007-2008 Jorge Luis Zapata
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
typedef struct _Hswitch
{
	Enesim_Renderer base;
	unsigned int w;
	unsigned int h;
	Enesim_Renderer *lrend;
	Enesim_Renderer *rrend;
	double step;
} Hswitch;

static inline Hswitch * _hswitch_get(Enesim_Renderer *r)
{
	Hswitch *thiz;

	thiz = enesim_renderer_data_get(r);
	return thiz;
}

static void _generic_good(Enesim_Renderer *r, int x, int y, unsigned int len, uint32_t *dst)
{
	Hswitch *hs;
	uint32_t *end = dst + len;
	Eina_F16p16 mmx;
	int mx;

	hs = _hswitch_get(r);
	mmx = eina_f16p16_double_from(hs->w - (double)(hs->w * hs->step));
	mx = eina_f16p16_int_to(mmx);
	while (dst < end)
	{
		uint32_t p0;

		if (x > mx)
		{
			hs->rrend->sw_fill(hs->rrend, x, y, 1, &p0);
		}
		else if (x < mx)
		{
			hs->lrend->sw_fill(hs->lrend, x, y, 1, &p0);
		}
		else
		{
			uint32_t p1;
			uint16_t a;

			a = 1 + ((mmx & 0xffff) >> 8);
			hs->lrend->sw_fill(hs->lrend, x, y, 1, &p0);
			hs->rrend->sw_fill(hs->rrend, 0, y, 1, &p1);
			p0 = argb8888_interp_256(a, p0, p1);
		}
		*dst++ = p0;
		x++;
	}
}

static void _affine_good(Enesim_Renderer *r, int x, int y, unsigned int len, uint32_t *dst)
{
	Hswitch *hs;
	uint32_t *end = dst + len;
	Eina_F16p16 mmx, xx, yy;
	int mx;

	hs = _hswitch_get(r);
	yy = eina_f16p16_int_from(y);
	xx = eina_f16p16_int_from(x);
	yy = eina_f16p16_mul(r->matrix.values.yx, xx) +
			eina_f16p16_mul(r->matrix.values.yy, yy) + r->matrix.values.yz;
	xx = eina_f16p16_mul(r->matrix.values.xx, xx) +
			eina_f16p16_mul(r->matrix.values.xy, yy) + r->matrix.values.xz;

	/* FIXME put this on the state setup */
	mmx = eina_f16p16_double_from(hs->w - (double)(hs->w * hs->step));
	mx = eina_f16p16_int_to(mmx);
	while (dst < end)
	{
		uint32_t p0;

		x = eina_f16p16_int_to(xx);
		y = eina_f16p16_int_to(yy);
		if (x > mx)
		{
			hs->rrend->sw_fill(hs->rrend, x, y, 1, &p0);
		}
		else if (x < mx)
		{
			hs->lrend->sw_fill(hs->lrend, x, y, 1, &p0);
		}
		/* FIXME, what should we use here? mmx or xx?
		 * or better use a subpixel center?
		 */
		else
		{
			uint32_t p1;
			uint16_t a;

			a = 1 + ((xx & 0xffff) >> 8);
			hs->lrend->sw_fill(hs->lrend, x, y, 1, &p0);
			hs->rrend->sw_fill(hs->rrend, 0, y, 1, &p1);
			p0 = argb8888_interp_256(a, p1, p0);
		}
		*dst++ = p0;
		xx += r->matrix.values.xx;
		yy += r->matrix.values.yx;
	}
}

static void _generic_fast(Enesim_Renderer *r, int x, int y, unsigned int len, uint32_t *dst)
{
	Hswitch *hs;
	int mx;
	Eina_Rectangle ir, dr;

	hs = _hswitch_get(r);
	eina_rectangle_coords_from(&ir, x, y, len, 1);
	eina_rectangle_coords_from(&dr, 0, 0, hs->w, hs->h);
	if (!eina_rectangle_intersection(&ir, &dr))
		return;

	mx = hs->w - (hs->w * hs->step);
	if (mx == 0)
	{
		hs->rrend->sw_fill(hs->rrend, ir.x, ir.y, ir.w, dst);
	}
	else if (mx == hs->w)
	{
		hs->lrend->sw_fill(hs->lrend, ir.x, ir.y, ir.w, dst);
	}
	else
	{
		if (ir.x > mx)
		{
			hs->rrend->sw_fill(hs->rrend, ir.x, ir.y, ir.w, dst);
		}
		else if (ir.x + ir.w < mx)
		{
			hs->lrend->sw_fill(hs->lrend, ir.x, ir.y, ir.w, dst);
		}
		else
		{
			int w;

			w = mx - ir.x;
			hs->lrend->sw_fill(hs->lrend, ir.x, ir.y, w, dst);
			dst += w;
			hs->rrend->sw_fill(hs->rrend, 0, ir.y, ir.w + ir.x - mx , dst);
		}
	}
}

static Eina_Bool _state_setup(Enesim_Renderer *r, Enesim_Renderer_Sw_Fill *fill)
{
	Hswitch *h;

	h = _hswitch_get(r);
	if (!h->lrend || !h->rrend)
		return EINA_FALSE;
	if (!enesim_renderer_sw_setup(h->lrend))
		return EINA_FALSE;
	if (!enesim_renderer_sw_setup(h->rrend))
		return EINA_FALSE;

	*fill = _affine_good;
	return EINA_TRUE;
}

static void _free(Enesim_Renderer *r)
{
	Hswitch *thiz;

	thiz = _hswitch_get(r);
	free(thiz);
}


static Enesim_Renderer_Descriptor _descriptor = {
	/* .sw_setup =   */ _state_setup,
	/* .sw_cleanup = */ NULL,
	/* .free =       */ _free,
	/* .boundings =  */ NULL,
	/* .flags =      */ NULL,
	/* .is_inside =  */ 0
};
/*============================================================================*
 *                                 Global                                     *
 *============================================================================*/
/*============================================================================*
 *                                   API                                      *
 *============================================================================*/
/**
 * Creates a new horizontal switch renderer
 *
 * A horizontal switch renderer renders an horizontal translation between
 * the left and right renderers based on the step value.
 * @return The renderer
 */
EAPI Enesim_Renderer * enesim_renderer_hswitch_new(void)
{
	Hswitch *thiz;
	Enesim_Renderer *r;

	thiz = calloc(1, sizeof(Hswitch));
	r = enesim_renderer_new(&_descriptor, thiz);

	return r;
}
/**
 * Sets the width of the renderer window
 * @param[in] r The horizontal switch renderer
 * @param[in] w The width
 */
EAPI void enesim_renderer_hswitch_w_set(Enesim_Renderer *r, int w)
{
	Hswitch *hs;

	hs = _hswitch_get(r);
	if (hs->w == w)
		return;
	hs->w = w;
}
/**
 * Sets the height of the renderer window
 * @param[in] r The horizontal switch renderer
 * @param[in] h The height
 */
EAPI void enesim_renderer_hswitch_h_set(Enesim_Renderer *r, int h)
{
	Hswitch *hs;

	hs = _hswitch_get(r);
	if (hs->h == h)
		return;
	hs->h = h;
}
/**
 * Sets the left renderer
 * @param[in] r The horizontal switch renderer
 * @param[in] left The left renderer
 */
EAPI void enesim_renderer_hswitch_left_set(Enesim_Renderer *r,
		Enesim_Renderer *left)
{
	Hswitch *hs;

	hs = _hswitch_get(r);
	hs->lrend = left;
}
/**
 * Sets the right renderer
 * @param[in] r The horizontal switch renderer
 * @param[in] right The right renderer
 */
EAPI void enesim_renderer_hswitch_right_set(Enesim_Renderer *r,
		Enesim_Renderer *right)
{
	Hswitch *hs;

	hs = _hswitch_get(r);
	hs->rrend = right;
}
/**
 * Sets the step
 * @param[in] r The horizontal switch renderer
 * @param[in] step The step. A value of 0 will render the left
 * renderer, a value of 1 will render the right renderer
 */
EAPI void enesim_renderer_hswitch_step_set(Enesim_Renderer *r, double step)
{
	Hswitch *hs;

	if (step < 0)
		step = 0;
	else if (step > 1)
		step = 1;
	hs = _hswitch_get(r);
	hs->step = step;
}
