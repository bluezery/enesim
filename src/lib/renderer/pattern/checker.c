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
/**
 * @todo
 * - Optimize the case where both colors are the same
 * - There's a bug on the affine renderer, easily reproducible whenever we scale
 */
/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/
typedef struct _Checker
{
	Enesim_Color color1;
	Enesim_Color color2;
	int sw;
	int sh;
} Checker;

Checker * _checker_get(Enesim_Renderer *r)
{
	Checker *thiz;

	thiz = enesim_renderer_data_get(r);
	return thiz;
}

static void _span_identity(Enesim_Renderer *r, int x, int y, unsigned int len, uint32_t *dst)
{
	Checker *thiz;
	Eina_F16p16 yy, xx;
	int w2;
	int h2;;
	uint32_t color[2];
	uint32_t *end = dst + len;
	int sy;

	thiz = _checker_get(r);
	w2 = thiz->sw * 2;
	h2 = thiz->sh * 2;
	color[0] = thiz->color1;
	color[1] = thiz->color2;

	/* translate to the origin */
	renderer_identity_setup(r, x, y, &xx, &yy);
	/* normalize the modulo */
	sy = ((yy  >> 16) % h2);
	if (sy < 0)
	{
		sy += h2;
	}
	/* swap the colors */
	if (sy >= thiz->sh)
	{
		color[0] = thiz->color2;
		color[1] = thiz->color1;
	}
	while (dst < end)
	{
		int sx;
		uint32_t p0;

		sx = ((xx >> 16) % w2);
		if (sx < 0)
		{
			sx += w2;
		}
		/* choose the correct color */
		if (sx >= thiz->sw)
		{
			p0 = color[0];
		}
		else
		{
			p0 = color[1];
		}
		*dst++ = p0;
		xx += EINA_F16P16_ONE;
	}
}

static void _span_affine(Enesim_Renderer *r, int x, int y, unsigned int len, uint32_t *dst)
{
	Checker *thiz;
	Eina_F16p16 yy, xx, ww, hh, ww2, hh2;
	uint32_t *end = dst + len;

	thiz = _checker_get(r);
	renderer_affine_setup(r, x, y, &xx, &yy);
	/* TODO move this to the state setup */
	ww = eina_f16p16_int_from(thiz->sw);
	ww2 = ww * 2;
	hh = eina_f16p16_int_from(thiz->sh);
	hh2 = hh * 2;

	while (dst < end)
	{
		Eina_F16p16 syy, sxx;
		uint32_t color[2] = {thiz->color1, thiz->color2};
		uint32_t p0;
		int sx, sy;

		/* normalize the modulo */
		syy = (yy % hh2);
		if (syy < 0)
		{
			syy += hh2;
		}
		sxx = (xx % ww2);
		if (sxx < 0)
		{
			sxx += ww2;
		}
		sy = eina_f16p16_int_to(syy);
		sx = eina_f16p16_int_to(sxx);
		/* choose the correct color */
		if (syy >= hh)
		{
			color[0] = thiz->color2;
			color[1] = thiz->color1;
		}
		if (sxx >= ww)
		{
			p0 = color[0];
			/* antialias the borders */
			if (sy == 0 || sy == thiz->sh)
			{
				uint16_t a;

				a = 1 + ((syy & 0xffff) >> 8);
				p0 = INTERP_256(a, p0, color[1]);
			}
			if (sx == 0 || sx == thiz->sw)
			{
				uint16_t a;

				a = 1 + ((sxx & 0xffff) >> 8);
				p0 = INTERP_256(a, p0, color[1]);
			}
		}
		else
		{
			p0 = color[1];

			/* antialias the borders */
			if (sy == 0 || sy == thiz->sh)
			{
				uint16_t a;

				a = 1 + ((syy & 0xffff) >> 8);
				p0 = INTERP_256(a, p0, color[0]);
			}
			if (sx == 0 || sx == thiz->sw)
			{
				uint16_t a;

				a = 1 + ((sxx & 0xffff) >> 8);
				p0 = INTERP_256(a, p0, color[0]);
			}
		}
		yy += r->matrix.values.yx;
		xx += r->matrix.values.xx;
		*dst++ = p0;
	}
}

static void _span_projective(Enesim_Renderer *r, int x, int y, unsigned int len, uint32_t *dst)
{
	Checker *thiz;
	Eina_F16p16 yy, xx, ww, hh, ww2, hh2, zz;
	uint32_t *end = dst + len;

	thiz = _checker_get(r);
	/* translate to the origin */
	renderer_projective_setup(r, x, y, &xx, &yy, &zz);
	/* TODO move this to the state setup */
	ww = eina_f16p16_int_from(thiz->sw);
	ww2 = ww * 2;
	hh = eina_f16p16_int_from(thiz->sh);
	hh2 = hh * 2;

	while (dst < end)
	{
		Eina_F16p16 syy, sxx, syyy, sxxx;
		uint32_t color[2] = { thiz->color1, thiz->color2};
		uint32_t p0;
		int sx, sy;

		syyy = ((((int64_t)yy) << 16) / zz);
		sxxx = ((((int64_t)xx) << 16) / zz);
		/* normalize the modulo */
		syy = (syyy % hh2);
		if (syy < 0)
		{
			syy += hh2;
		}
		sxx = (sxxx % ww2);
		if (sxx < 0)
		{
			sxx += ww2;
		}
		sy = eina_f16p16_int_to(syy);
		sx = eina_f16p16_int_to(sxx);
		/* choose the correct color */
		if (syy >= hh)
		{
			color[0] = thiz->color2;
			color[1] = thiz->color1;
		}
		if (sxx >= ww)
		{
			p0 = color[0];

			/* antialias the borders */
			if (sy == 0 || sy == thiz->sh)
			{
				uint16_t a;

				a = 1 + ((syy & 0xffff) >> 8);
				p0 = INTERP_256(a, p0, color[1]);
			}
			if (sx == 0 || sx == thiz->sw)
			{
				uint16_t a;

				a = 1 + ((sxx & 0xffff) >> 8);
				p0 = INTERP_256(a, p0, color[1]);
			}
		}
		else
		{
			p0 = color[1];
			/* antialias the borders */
			if (sy == 0 || sy == thiz->sh)
			{
				uint16_t a;

				a = 1 + ((syy & 0xffff) >> 8);
				p0 = INTERP_256(a, p0, color[0]);
			}
			if (sx == 0 || sx == thiz->sw)
			{
				uint16_t a;

				a = 1 + ((sxx & 0xffff) >> 8);
				p0 = INTERP_256(a, p0, color[0]);
			}
		}
		yy += r->matrix.values.yx;
		xx += r->matrix.values.xx;
		zz += r->matrix.values.zx;
		*dst++ = p0;
	}
}
/*----------------------------------------------------------------------------*
 *                      The Enesim's renderer interface                       *
 *----------------------------------------------------------------------------*/
static void _checker_state_cleanup(Enesim_Renderer *r)
{

}

static Eina_Bool _checker_state_setup(Enesim_Renderer *r, Enesim_Renderer_Sw_Fill *fill)
{
	Checker *thiz;

	thiz = _checker_get(r);

	if (r->matrix.type == ENESIM_MATRIX_IDENTITY)
		*fill = _span_identity;
	else if (r->matrix.type == ENESIM_MATRIX_AFFINE)
		*fill = _span_affine;
	else
		*fill = _span_projective;
	return EINA_TRUE;
}

static void _checker_free(Enesim_Renderer *r)
{
	Checker *thiz;

	thiz = _checker_get(r);
	free(thiz);
}

static void _checker_flags(Enesim_Renderer *r, Enesim_Renderer_Flag *flags)
{
	Checker *thiz;

	thiz = _checker_get(r);
	if (!thiz)
	{
		*flags = 0;
		return;
	}

	*flags = ENESIM_RENDERER_FLAG_AFFINE |
			ENESIM_RENDERER_FLAG_PERSPECTIVE |
			ENESIM_RENDERER_FLAG_ARGB8888;
}

static Enesim_Renderer_Descriptor _descriptor = {
	.sw_setup = _checker_state_setup,
	.sw_cleanup = _checker_state_cleanup,
	.flags = _checker_flags,
	.free = _checker_free,
};
/*============================================================================*
 *                                 Global                                     *
 *============================================================================*/
/*============================================================================*
 *                                   API                                      *
 *============================================================================*/
/**
 * Creates a checker renderer
 * @return The new renderer
 */
EAPI Enesim_Renderer * enesim_renderer_checker_new(void)
{
	Enesim_Renderer *r;
	Checker *thiz;

	thiz = calloc(1, sizeof(Checker));
	if (!thiz) return NULL;

	/* specific renderer setup */
	thiz->sw = 1;
	thiz->sh = 1;
	r = enesim_renderer_new(&_descriptor, thiz);

	return r;
}
/**
 * Sets the color of the even squares
 * @param[in] r The checker renderer
 * @param[in] color The color
 */
EAPI void enesim_renderer_checker_even_color_set(Enesim_Renderer *r, Enesim_Color color)
{
	Checker *thiz;

	thiz = _checker_get(r);

	thiz->color1 = color;
}
/**
 * Gets the color of the even squares
 * @param[in] r The checker renderer
 * @return The color
 */
EAPI Enesim_Color enesim_renderer_checker_even_color_get(Enesim_Renderer *r)
{
	Checker *thiz;

	thiz = _checker_get(r);

	return thiz->color1;
}
/**
 * Sets the color of the odd squares
 * @param[in] r The checker renderer
 * @param[in] color The color
 */
EAPI void enesim_renderer_checker_odd_color_set(Enesim_Renderer *r, Enesim_Color color)
{
	Checker *thiz;

	thiz = _checker_get(r);

	thiz->color2 = color;
}
/**
 * Gets the color of the odd squares
 * @param[in] r The checker renderer
 * @return The color
 */
EAPI Enesim_Color enesim_renderer_checker_odd_color_get(Enesim_Renderer *r)
{
	Checker *thiz;

	thiz = _checker_get(r);

	return thiz->color2;
}
/**
 * Sets the width of the checker rectangles
 * @param[in] width The width
 */
EAPI void enesim_renderer_checker_width_set(Enesim_Renderer *r, int width)
{
	Checker *thiz;

	thiz = _checker_get(r);

	thiz->sw = width;
}
/**
 * Gets the width of the checker rectangles
 * @returns The width
 */
EAPI int enesim_renderer_checker_width_get(Enesim_Renderer *r)
{
	Checker *thiz;

	thiz = _checker_get(r);

	return thiz->sw;
}
/**
 * Sets the height of the checker rectangles
 * @param[in] height The height
 */
EAPI void enesim_renderer_checker_height_set(Enesim_Renderer *r, int height)
{
	Checker *thiz;

	thiz = _checker_get(r);

	thiz->sh = height;
}
/**
 * Gets the height of the checker rectangles
 * @returns The height
 */
EAPI int enesim_renderer_checker_heigth_get(Enesim_Renderer *r)
{
	Checker *thiz;

	thiz = _checker_get(r);

	return thiz->sh;
}

