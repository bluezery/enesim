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
/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/
typedef struct _Enesim_Renderer_Circle_State {
	double x;
	double y;
	double r;
} Enesim_Renderer_Circle_State;

typedef struct _Enesim_Renderer_Circle {
	/* public properties */
	Enesim_Renderer_Circle_State current;
	Enesim_Renderer_Circle_State past;
	/* private */
	Eina_Bool changed : 1;
	/* internal state */
	Enesim_F16p16_Matrix matrix;
	int xx0, yy0;
	int rr0, irr0;
	unsigned char do_inner :1;
} Enesim_Renderer_Circle;

static inline Enesim_Renderer_Circle * _circle_get(Enesim_Renderer *r)
{
	Enesim_Renderer_Circle *thiz;

	thiz = enesim_renderer_shape_data_get(r);
	return thiz;
}

static void _stroked_fill_paint(Enesim_Renderer *r, int x, int y,
		unsigned int len, void *ddata)
{
	Enesim_Renderer_Circle *thiz = _circle_get(r);
	Enesim_Shape_Draw_Mode draw_mode;
	Enesim_Renderer *fpaint;
	int axx = thiz->matrix.xx;
	int ayx = thiz->matrix.yx;
	int do_inner = thiz->do_inner;
	unsigned int ocolor;
	unsigned int icolor;
	int rr0 = thiz->rr0, rr1 = rr0 + 65536;
	int irr0 = thiz->irr0, irr1 = irr0 + 65536;
	int rr2 = rr1 * 1.41421357, irr2 = irr1 * 1.41421357; // sqrt(2)
	int xx0 = thiz->xx0, yy0 = thiz->yy0;
	uint32_t *dst = ddata;
	unsigned int *d = dst, *e = d + len;
	int xx, yy;
	int fill_only = 0;

	enesim_renderer_shape_stroke_color_get(r, &ocolor);
	enesim_renderer_shape_fill_color_get(r, &icolor);
 	enesim_renderer_shape_fill_renderer_get(r, &fpaint);
	enesim_renderer_shape_draw_mode_get(r, &draw_mode);

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
			Enesim_Renderer_Sw_Data *sdata;

			sdata = enesim_renderer_backend_data_get(fpaint, ENESIM_BACKEND_SOFTWARE);
			sdata->fill(fpaint, x, y, len, dst);
		}
	}
	if ((draw_mode == ENESIM_SHAPE_DRAW_MODE_STROKE_FILL) && do_inner
			&& fpaint)
	{
		Enesim_Renderer_Sw_Data *sdata;

		sdata = enesim_renderer_backend_data_get(fpaint, ENESIM_BACKEND_SOFTWARE);
		sdata->fill(fpaint, x, y, len, dst);
	}

        enesim_renderer_affine_setup(r, x, y, &thiz->matrix, &xx, &yy);
	xx -= xx0;
	yy -= yy0;
	while (d < e)
	{
		unsigned int q0 = 0;

		if ((abs(xx) <= rr1) && (abs(yy) <= rr1))
		{
			unsigned int op0 = ocolor, p0;
			int a = 256;

			if (fill_only && fpaint)
				op0 = argb8888_mul4_sym(*d, op0);

			if (abs(xx) + abs(yy) >= rr0)
			{
				a = 0;
				if (abs(xx) + abs(yy) <= rr2)
				{
					int rr = hypot(xx, yy);

					if (rr < rr1)
					{
						a = 256;
						if (rr > rr0)
							a -= ((rr - rr0) >> 8);
					}
				}
			}

			if (a < 256)
				op0 = argb8888_mul_256(a, op0);

			p0 = op0;
			if (do_inner && (abs(xx) <= irr1) && (abs(yy) <= irr1))
			{
				p0 = icolor;
				if (fpaint)
				{
					p0 = *d;
					if (icolor != 0xffffffff)
						p0 = argb8888_mul4_sym(icolor, p0);
				}
				a = 256;
				if (abs(xx) + abs(yy) >= irr0)
				{
					a = 0;
					if (abs(xx) + abs(yy) <= irr2)
					{
						int rr = hypot(xx, yy);

						if (rr < irr1)
						{
							a = 256;
							if (rr > irr0)
								a -= ((rr - irr0) >> 8);
						}
					}
				}

				if (a < 256)
					p0 = argb8888_interp_256(a, p0, op0);
			}
			q0 = p0;
		}
		*d++ = q0;
		xx += axx;
		yy += ayx;
	}
}
/*----------------------------------------------------------------------------*
 *                      The Enesim's renderer interface                       *
 *----------------------------------------------------------------------------*/
static const char * _circle_name(Enesim_Renderer *r)
{
	return "circle";
}

static void _boundings(Enesim_Renderer *r, Enesim_Rectangle *rect)
{
	Enesim_Renderer_Circle *thiz;

	thiz = _circle_get(r);
	rect->x = thiz->current.x - thiz->current.r;
	rect->y = thiz->current.y - thiz->current.r;
	rect->w = rect->h = thiz->current.r * 2;
}

static Eina_Bool _state_setup(Enesim_Renderer *r,
		const Enesim_Renderer_State *state,
		Enesim_Surface *s,
		Enesim_Renderer_Sw_Fill *fill, Enesim_Error **error)
{
	Enesim_Renderer_Circle *thiz;
	double rad;
	double sw;

	thiz = _circle_get(r);
	if (!thiz || (thiz->current.r < 1))
		return EINA_FALSE;

	thiz->rr0 = 65536 * (thiz->current.r - 1);
	thiz->xx0 = 65536 * (thiz->current.x - 0.5);
	thiz->yy0 = 65536 * (thiz->current.y - 0.5);

	enesim_renderer_shape_stroke_weight_get(r, &sw);
	thiz->do_inner = 1;
	if (sw >= (thiz->current.r - 1))
	{
		sw = 0;
		thiz->do_inner = 0;
	}
	rad = thiz->current.r - 1 - sw;
	if (rad < 0.0039)
		rad = 0;

	thiz->irr0 = rad * 65536;

	if (!enesim_renderer_shape_setup(r, state, s, error))
		return EINA_FALSE;

	enesim_matrix_f16p16_matrix_to(&state->transformation,
			&thiz->matrix);

	*fill = _stroked_fill_paint;

	return EINA_TRUE;
}

static void _state_cleanup(Enesim_Renderer *r, Enesim_Surface *s)
{
	Enesim_Renderer_Circle *thiz;

	thiz = _circle_get(r);
	enesim_renderer_shape_cleanup(r, s);
	thiz->past = thiz->current;
	thiz->changed = EINA_FALSE;
}

static void _flags(Enesim_Renderer *r, Enesim_Renderer_Flag *flags)
{
	Enesim_Renderer_Circle *thiz;

	thiz = _circle_get(r);
	if (!thiz)
	{
		*flags = 0;
		return;
	}

	*flags = ENESIM_RENDERER_FLAG_TRANSLATE |
			ENESIM_RENDERER_FLAG_AFFINE |
			ENESIM_RENDERER_FLAG_ARGB8888;
}

static Eina_Bool _circle_has_changed(Enesim_Renderer *r)
{
	Enesim_Renderer_Circle *thiz;

	thiz = _circle_get(r);
	if (!thiz->changed) return EINA_FALSE;
	/* the radius */
	if (thiz->current.r != thiz->past.r)
		return EINA_TRUE;
	/* the x */
	if (thiz->current.x != thiz->past.x)
		return EINA_TRUE;
	/* the y */
	if (thiz->current.y != thiz->past.y)
		return EINA_TRUE;
	return EINA_FALSE;
}

static void _free(Enesim_Renderer *r)
{
}

static Enesim_Renderer_Descriptor _circle_descriptor = {
	/* .version = 			*/ ENESIM_RENDERER_API,
	/* .name = 			*/ _circle_name,
	/* .free = 			*/ _free,
	/* .boundings = 		*/ _boundings,
	/* .destination_transform = 	*/ NULL,
	/* .flags = 			*/ _flags,
	/* .is_inside = 		*/ NULL,
	/* .damage = 			*/ NULL,
	/* .has_changed = 		*/ _circle_has_changed,
	/* .sw_setup = 			*/ _state_setup,
	/* .sw_cleanup = 		*/ _state_cleanup,
	/* .opencl_setup =		*/ NULL,
	/* .opencl_kernel_setup =	*/ NULL,
	/* .opencl_cleanup =		*/ NULL
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
EAPI Enesim_Renderer * enesim_renderer_circle_new(void)
{
	Enesim_Renderer_Circle *thiz;
	Enesim_Renderer *r;

	thiz = calloc(1, sizeof(Enesim_Renderer_Circle));
	if (!thiz) return NULL;
	r = enesim_renderer_shape_new(&_circle_descriptor, thiz);
	return r;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_circle_x_set(Enesim_Renderer *r, double x)
{
	Enesim_Renderer_Circle *thiz;

	thiz = _circle_get(r);
	thiz->current.x = x;
	thiz->changed = EINA_TRUE;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_circle_y_set(Enesim_Renderer *r, double y)
{
	Enesim_Renderer_Circle *thiz;

	thiz = _circle_get(r);
	thiz->current.y = y;
	thiz->changed = EINA_TRUE;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_circle_center_set(Enesim_Renderer *r, double x, double y)
{
	Enesim_Renderer_Circle *thiz;

	thiz = _circle_get(r);
	thiz->current.x = x;
	thiz->current.y = y;
	thiz->changed = EINA_TRUE;
}
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_circle_center_get(Enesim_Renderer *r, double *x, double *y)
{
	Enesim_Renderer_Circle *thiz;

	thiz = _circle_get(r);
	if (x) *x = thiz->current.x;
	if (y) *y = thiz->current.y;
}
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_circle_radius_set(Enesim_Renderer *r, double radius)
{
	Enesim_Renderer_Circle *thiz;

	thiz = _circle_get(r);
	if (radius < 1)
		radius = 1;
	thiz->current.r = radius;
	thiz->changed = EINA_TRUE;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_circle_radius_get(Enesim_Renderer *r, double *radius)
{
	Enesim_Renderer_Circle *thiz;

	thiz = _circle_get(r);
	if (radius) *radius = thiz->current.r;
}
