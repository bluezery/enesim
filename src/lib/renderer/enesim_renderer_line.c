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
/* FIXME use the ones from libargb */
#define MUL_A_256(a, c) \
 ( (((((c) >> 8) & 0x00ff00ff) * (a)) & 0xff00ff00) + \
   (((((c) & 0x00ff00ff) * (a)) >> 8) & 0x00ff00ff) )

#define MUL4_SYM(x, y) \
 ( ((((((x) >> 16) & 0xff00) * (((y) >> 16) & 0xff00)) + 0xff0000) & 0xff000000) + \
   ((((((x) >> 8) & 0xff00) * (((y) >> 16) & 0xff)) + 0xff00) & 0xff0000) + \
   ((((((x) & 0xff00) * ((y) & 0xff00)) + 0xff00) >> 16) & 0xff00) + \
   (((((x) & 0xff) * ((y) & 0xff)) + 0xff) >> 8) )


typedef struct _Enesim_Renderer_Line_State
{
	double x0;
	double y0;
	double x1;
	double y1;
} Enesim_Renderer_Line_State;

typedef struct _Enesim_Renderer_Line
{
	/* properties */
	Enesim_Renderer_Line_State current;
	/* private */
	Enesim_Renderer_Line_State past;
	Eina_Bool changed : 1;
	Enesim_F16p16_Matrix matrix;

	Enesim_F16p16_Line line;
	Enesim_F16p16_Line np;
	Enesim_F16p16_Line nm;

	Eina_F16p16 rr;
	Eina_F16p16 lxx, rxx, tyy, byy;
} Enesim_Renderer_Line;

static inline Enesim_Renderer_Line * _line_get(Enesim_Renderer *r)
{
	Enesim_Renderer_Line *thiz;

	thiz = enesim_renderer_shape_data_get(r);
	return thiz;
}

static void _span(Enesim_Renderer *r, int x, int y,
		unsigned int len, void *ddata)
{
	Enesim_Renderer_Line *thiz;
	Enesim_Renderer *srend = NULL;
	Enesim_Color scolor;
	Eina_F16p16 rr;
	/* FIXME use eina_f16p16 */
	long long int axx, axy, axz;
	long long int ayx, ayy, ayz;
	Eina_F16p16 d01;
	Eina_F16p16 d01_np;
	Eina_F16p16 d01_nm;
	Eina_F16p16 e01;
	Eina_F16p16 e01_np;
	Eina_F16p16 e01_nm;
	/* FIXME use eina_f16p16 */
	long long int xx;
	long long int yy;
	uint32_t *dst = ddata;
	uint32_t *d = dst;
	uint32_t *e = d + len;

	thiz = _line_get(r);

	rr = thiz->rr;
	axx = thiz->matrix.xx, axy = thiz->matrix.xy, axz = thiz->matrix.xz;
	ayx = thiz->matrix.yx, ayy = thiz->matrix.yy, ayz = thiz->matrix.yz;

	d01 = ((thiz->line.a * axx) >> 16) + ((thiz->line.b * ayx) >> 16);
	d01_np = ((thiz->np.a * axx) >> 16) + ((thiz->np.b * ayx) >> 16);
 	d01_nm = ((thiz->nm.a * axx) >> 16) + ((thiz->nm.b * ayx) >> 16);

	xx = (axx * x) + (axx >> 1) + (axy * y) + (axy >> 1) + axz - 32768;
	yy = (ayx * x) + (ayx >> 1) + (ayy * y) + (ayy >> 1) + ayz - 32768;

	e01 = ((thiz->line.a * xx) >> 16) + ((thiz->line.b * yy) >> 16) + thiz->line.c;
	e01_np = ((thiz->np.a * xx) >> 16) + ((thiz->np.b * yy) >> 16) + thiz->np.c;
	e01_nm = ((thiz->nm.a * xx) >> 16) + ((thiz->nm.b * yy) >> 16) + thiz->nm.c;

	enesim_renderer_shape_stroke_color_get(r, &scolor);
	enesim_renderer_shape_stroke_renderer_get(r, &srend);

	if (srend)
	{
			Enesim_Renderer_Sw_Data *sdata;

			sdata = enesim_renderer_backend_data_get(srend, ENESIM_BACKEND_SOFTWARE);
			sdata->fill(srend, x, y, len, ddata);
	}

	while (d < e)
	{
		uint32_t p0 = 0;

		if ((abs(e01) <= rr) && (e01_np >= 0) && (e01_nm >= 0))
		{
			int a = 256;

			p0 = scolor;
			if (srend)
			{
				p0 = *d;
				if (scolor != 0xffffffff)
					p0 = MUL4_SYM(p0, scolor);
			}

			/* anti-alias the edges */
			if (((rr - e01) >> 16) == 0)
				a = 1 + (((rr - e01) & 0xffff) >> 8);
			if (((e01 + rr) >> 16) == 0)
				a = (a * (1 + ((e01 + rr) & 0xffff))) >> 16;
			if ((e01_np >> 16) == 0)
				a = (a * (1 + (e01_np & 0xffff))) >> 16;
			if ((e01_nm >> 16) == 0)
				a = ((a * (1 + (e01_nm & 0xffff))) >> 16);

			if (a < 256)
				p0 = MUL_A_256(a, p0);
		}
		*d++ = p0;
		e01 += d01;
		e01_np += d01_np;
		e01_nm += d01_nm;
	}
}
/*----------------------------------------------------------------------------*
 *                      The Enesim's renderer interface                       *
 *----------------------------------------------------------------------------*/
static const char * _line_name(Enesim_Renderer *r)
{
	return "line";
}

static Eina_Bool _line_state_setup(Enesim_Renderer *r,
		const Enesim_Renderer_State *state,
		Enesim_Surface *s,
		Enesim_Renderer_Sw_Fill *fill, Enesim_Error **error)
{
	Enesim_Renderer_Line *thiz;
	Enesim_F16p16_Point p0, p1;
	Eina_F16p16 vx, vy;
	double x0, x1, y0, y1;
	double x01, y01;
	double len;
	double stroke;

	thiz = _line_get(r);

	x0 = thiz->current.x0;
	x1 = thiz->current.x1;
	y0 = thiz->current.y0;
	y1 = thiz->current.y1;

	if (y1 < y0)
	{
		thiz->byy = eina_f16p16_double_from(y0);
		thiz->tyy = eina_f16p16_double_from(y1);
	}
	else
	{
		thiz->byy = eina_f16p16_double_from(y1);
		thiz->tyy = eina_f16p16_double_from(y0);
	}

	if (x1 < x0)
	{
		thiz->rxx = eina_f16p16_double_from(x0);
		thiz->lxx = eina_f16p16_double_from(x1);
	}
	else
	{
		thiz->rxx = eina_f16p16_double_from(x1);
		thiz->lxx = eina_f16p16_double_from(x0);
	}

	x01 = x1 - x0;
	y01 = y1 - y0;

	if ((len = hypot(x01, y01)) < 1)
		return EINA_FALSE;

	vx = eina_f16p16_double_from(x01);
	vy = eina_f16p16_double_from(y01);
	p0.x = eina_f16p16_double_from(x0);
	p0.y = eina_f16p16_double_from(y0);
	p1.x = eina_f16p16_double_from(x1);
	p1.y = eina_f16p16_double_from(y1);

	/* normalize to line length so that aa works well */
	/* the original line */
	enesim_f16p16_line_f16p16_direction_from(&thiz->line, &p0, vx, vy);
	thiz->line.a /= len;
	thiz->line.b /= len;
	thiz->line.c /= len;
	/* the perpendicular line on the initial point */
	enesim_f16p16_line_f16p16_direction_from(&thiz->np, &p0, -vy, vx);
	thiz->np.a /= len;
	thiz->np.b /= len;
	thiz->np.c /= len;
	/* the perpendicular line on the last point */
	enesim_f16p16_line_f16p16_direction_from(&thiz->nm, &p1, vy, -vx);
	thiz->nm.a /= len;
	thiz->nm.b /= len;
	thiz->nm.c /= len;

	enesim_renderer_shape_stroke_weight_get(r, &stroke);
	thiz->rr = EINA_F16P16_HALF * (stroke + 1);
	if (thiz->rr < EINA_F16P16_HALF) thiz->rr = EINA_F16P16_HALF;

	enesim_matrix_f16p16_matrix_to(&state->transformation,
			&thiz->matrix);
	*fill = _span;

	return EINA_TRUE;
}

static void _line_state_cleanup(Enesim_Renderer *r, Enesim_Surface *s)
{
	Enesim_Renderer_Line *thiz;

	thiz = _line_get(r);

	enesim_renderer_shape_cleanup(r, s);
	thiz->past = thiz->current;
	thiz->changed = EINA_FALSE;
}

static void _line_flags(Enesim_Renderer *r, Enesim_Renderer_Flag *flags)
{
	*flags = ENESIM_RENDERER_FLAG_TRANSLATE |
			ENESIM_RENDERER_FLAG_AFFINE |
			ENESIM_RENDERER_FLAG_ARGB8888;
}

static void _line_free(Enesim_Renderer *r)
{
}

static void _line_boundings(Enesim_Renderer *r, Enesim_Rectangle *boundings)
{
	Enesim_Renderer_Line *thiz;

	thiz = _line_get(r);

	boundings->x = thiz->current.x0;
	boundings->y = thiz->current.y0;
	boundings->w = fabs(thiz->current.x1 - thiz->current.x0);
	boundings->h = fabs(thiz->current.y1 - thiz->current.y0);
}

static Eina_Bool _line_has_changed(Enesim_Renderer *r)
{
	Enesim_Renderer_Line *thiz;

	thiz = _line_get(r);

	if (!thiz->changed) return EINA_FALSE;

	/* x0 */
	if (thiz->current.x0 != thiz->past.x0)
		return EINA_TRUE;
	/* y0 */
	if (thiz->current.y0 != thiz->past.y0)
		return EINA_TRUE;
	/* x1 */
	if (thiz->current.x1 != thiz->past.x1)
		return EINA_TRUE;
	/* y1 */
	if (thiz->current.y1 != thiz->past.y1)
		return EINA_TRUE;

	return EINA_FALSE;
}


static Enesim_Renderer_Descriptor _line_descriptor = {
	/* .version = 			*/ ENESIM_RENDERER_API,
	/* .name = 			*/ _line_name,
	/* .free = 			*/ _line_free,
	/* .boundings = 		*/ NULL,// _line_boundings,
	/* .destination_transform = 	*/ NULL,
	/* .flags = 			*/ _line_flags,
	/* .is_inside = 		*/ NULL,
	/* .damage = 			*/ NULL,
	/* .has_changed = 		*/ _line_has_changed,
	/* .sw_setup = 			*/ _line_state_setup,
	/* .sw_cleanup = 		*/ _line_state_cleanup
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
EAPI Enesim_Renderer * enesim_renderer_line_new(void)
{
	Enesim_Renderer *r;
	Enesim_Renderer_Line *thiz;

	thiz = calloc(1, sizeof(Enesim_Renderer_Line));
	if (!thiz) return NULL;
	r = enesim_renderer_shape_new(&_line_descriptor, thiz);
	return r;
}

EAPI void enesim_renderer_line_x0_set(Enesim_Renderer *r, double x0)
{
	Enesim_Renderer_Line *thiz;

	thiz = _line_get(r);
	thiz->current.x0 = x0;
	thiz->changed = EINA_TRUE;
}

EAPI void enesim_renderer_line_x0_get(Enesim_Renderer *r, double *x0)
{
	Enesim_Renderer_Line *thiz;

	thiz = _line_get(r);
	*x0 = thiz->current.x0;
}


EAPI void enesim_renderer_line_y0_set(Enesim_Renderer *r, double y0)
{
	Enesim_Renderer_Line *thiz;

	thiz = _line_get(r);
	thiz->current.y0 = y0;
	thiz->changed = EINA_TRUE;
}

EAPI void enesim_renderer_line_y0_get(Enesim_Renderer *r, double *y0)
{
	Enesim_Renderer_Line *thiz;

	thiz = _line_get(r);
	*y0 = thiz->current.y0;
}

EAPI void enesim_renderer_line_x1_set(Enesim_Renderer *r, double x1)
{
	Enesim_Renderer_Line *thiz;

	thiz = _line_get(r);
	thiz->current.x1 = x1;
	thiz->changed = EINA_TRUE;
}

EAPI void enesim_renderer_line_x1_get(Enesim_Renderer *r, double *x1)
{
	Enesim_Renderer_Line *thiz;

	thiz = _line_get(r);
	*x1 = thiz->current.x1;
}


EAPI void enesim_renderer_line_y1_set(Enesim_Renderer *r, double y1)
{
	Enesim_Renderer_Line *thiz;

	thiz = _line_get(r);
	thiz->current.y1 = y1;
	thiz->changed = EINA_TRUE;
}

EAPI void enesim_renderer_line_y1_get(Enesim_Renderer *r, double *y1)
{
	Enesim_Renderer_Line *thiz;

	thiz = _line_get(r);
	*y1 = thiz->current.y1;
}
