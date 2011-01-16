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
typedef struct _Background Background;

struct _Background {
	Enesim_Color color;
	Enesim_Color final_color;
	Enesim_Compositor_Span span;
};

static inline Background * _background_get(Enesim_Renderer *r)
{
	Background *s;

	s = enesim_renderer_data_get(r);
	return s;
}

static void _span(Enesim_Renderer *p, int x, int y,
		unsigned int len, uint32_t *dst)
{
	Background *bkg = _background_get(p);

	bkg->span(dst, len, NULL, bkg->final_color, NULL);
}

static Eina_Bool _setup_state(Enesim_Renderer *r, Enesim_Renderer_Sw_Fill *fill)
{
	Background *bkg = _background_get(r);
	Enesim_Format fmt = ENESIM_FORMAT_ARGB8888;
	Enesim_Rop rop;
	Enesim_Color final_color, rend_color;

	final_color = bkg->color;
	enesim_renderer_color_get(r, &rend_color);
	/* TODO multiply the bkg color with the rend color and use that for the span
	 */
	enesim_renderer_rop_get(r, &rop);
	bkg->final_color = final_color;
	bkg->span = enesim_compositor_span_get(rop, &fmt, ENESIM_FORMAT_NONE,
			final_color, ENESIM_FORMAT_NONE);
	*fill = _span;
	return EINA_TRUE;
}

static void _cleanup_state(Enesim_Renderer *p)
{
}

static void _free(Enesim_Renderer *p)
{
}

static Enesim_Renderer_Descriptor _descriptor = {
	.sw_setup = _setup_state,
	.sw_cleanup = _cleanup_state,
	.free = _free,
};

/*============================================================================*
 *                                   API                                      *
 *============================================================================*/
/**
 * Creates a background renderer
 * @return The new renderer
 */
EAPI Enesim_Renderer * enesim_renderer_background_new(void)
{
	Enesim_Renderer *r;
	Enesim_Renderer_Flag flags;
	Background *bkg;

	bkg = calloc(1, sizeof(Background));
	if (!bkg) return NULL;
	/* specific renderer setup */
	flags = ENESIM_RENDERER_FLAG_AFFINE |
			ENESIM_RENDERER_FLAG_PERSPECTIVE |
			ENESIM_RENDERER_FLAG_ARGB8888 |
			ENESIM_RENDERER_FLAG_ROP;
	r = enesim_renderer_new(&_descriptor, flags, bkg);
	return r;
}
/**
 * Sets the color of background
 * @param[in] r The background renderer
 * @param[in] color The background color
 */
EAPI void enesim_renderer_background_color_set(Enesim_Renderer *r,
		Enesim_Color color)
{
	Background *bkg;

	bkg = _background_get(r);
	bkg->color = color;
}

