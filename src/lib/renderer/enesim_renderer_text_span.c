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
#include "enesim_eina.h"
#include "enesim_error.h"
#include "enesim_color.h"
#include "enesim_rectangle.h"
#include "enesim_matrix.h"
#include "enesim_pool.h"
#include "enesim_buffer.h"
#include "enesim_surface.h"
#include "enesim_compositor.h"
#include "enesim_text.h"
#include "enesim_renderer.h"
#include "enesim_renderer_shape.h"
#include "enesim_renderer_shape.h"
#include "enesim_renderer_text_span.h"

#include "enesim_text_private.h"
#include "enesim_renderer_private.h"
#include "enesim_renderer_shape_private.h"
/**
 * @todo
 * Add a buffer interface, this way an API user might want to use each own
 * buffer implementation instead of always doing a strdup()
 *
 */
/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/
#define ENESIM_LOG_DEFAULT enesim_log_text

typedef struct _Enesim_Renderer_Text_Span_State
{
	struct {
		Enesim_Text_Buffer *buffer;
		unsigned int size;
		char *font_name;
	} current, past;
	Eina_Bool text_changed : 1;
	Eina_Bool changed : 1;
	Eina_Bool generated : 1;
} Enesim_Renderer_Text_Span_State;

typedef struct _Enesim_Renderer_Text_Span
{
	Enesim_Renderer_Text_Span_State state;
	Enesim_Text_Direction direction;
	Enesim_Text_Engine *engine;
	Enesim_Text_Font *font;
	unsigned int width;
	unsigned int height;
	int top;
	int bottom;
} Enesim_Renderer_Text_Span;

static Eina_Bool _enesim_renderer_text_span_has_changed(Enesim_Renderer *r);

static inline Enesim_Renderer_Text_Span * _enesim_renderer_text_span_get(Enesim_Renderer *r)
{
	Enesim_Renderer_Text_Span *thiz;

	thiz = enesim_renderer_shape_data_get(r);
	return thiz;
}

static Eina_Bool _enesim_renderer_text_span_changed(Enesim_Renderer_Text_Span *thiz)
{
	if (!thiz->state.changed)
		return EINA_FALSE;

	/* check that the current state is different from the past state */
	if (thiz->state.current.size != thiz->state.past.size)
	{
		return EINA_TRUE;
	}
	if ((thiz->state.current.font_name != thiz->state.past.font_name) ||
	(strcmp(thiz->state.current.font_name, thiz->state.past.font_name)))
	{
		return EINA_TRUE;
	}

	return EINA_FALSE;
}

static void _enesim_renderer_text_span_common_setup(Enesim_Renderer_Text_Span *thiz)
{
	if (_enesim_renderer_text_span_changed(thiz))
	{
		if (thiz->font)
		{
			enesim_text_font_unref(thiz->font);
			thiz->font = NULL;
		}
		if (thiz->state.current.font_name && thiz->state.current.size)
			thiz->font = enesim_text_font_load(thiz->engine, thiz->state.current.font_name, thiz->state.current.size);
		thiz->state.changed = EINA_FALSE;
	}
}

static void _enesim_renderer_text_span_setup(Enesim_Renderer *r)
{
	Enesim_Renderer_Text_Span *thiz;

	thiz = _enesim_renderer_text_span_get(r);
	_enesim_renderer_text_span_common_setup(thiz);
}

/* x and y must be inside the bounding box */
static inline Eina_Bool _enesim_renderer_text_span_get_glyph_at_ltr(Enesim_Renderer_Text_Span *thiz, Enesim_Text_Font *font, int x, int y, Enesim_Text_Glyph_Position *position)
{
	Eina_Bool ret = EINA_FALSE;
	int idx = 0;
	int rcoord = 0;
	const char *c;
	const char *text;

	text = enesim_text_buffer_string_get(thiz->state.current.buffer);
	for (c = text; c && *c; c++)
	{
		Enesim_Text_Glyph *g;
		int w, h;

		g = enesim_text_font_glyph_load(font, *c);
		if (!g)
		{
			WRN("No such glyph for %c", *c);
			continue;
		}
		if (!g->surface) goto advance;
		enesim_surface_size_get(g->surface, &w, &h);
		if (x >= rcoord && x <= rcoord + w)
		{
			position->glyph = g;
			position->index = idx;
			position->distance = rcoord;
			ret = EINA_TRUE;
			break;
		}
advance:
		rcoord += g->x_advance;
		idx++;
	}
	return ret;

}

#if 0
/* x and y must be inside the bounding box */
static inline Eina_Bool _enesim_renderer_text_span_get_glyph_at_rtl(Enesim_Renderer_Text_Span *thiz, Enesim_Text_Font *font, int x, int y, Enesim_Text_Glyph_Position *position)
{
	Eina_Bool ret = EINA_FALSE;
	int idx = 0;
	int rcoord = 0;
	const char *c;
	size_t len;
	const char *text;

	text = enesim_text_buffer_string_get(thiz->state.current.buffer);
	len = enesim_text_buffer_string_length(thiz->state.current.buffer);
	idx = len;
	for (c = text + len; c && c >= text; c--)
	{
		Enesim_Text_Glyph *g;
		int w, h;

		g = enesim_text_font_glyph_load(font, *c);
		if (!g)
		{
			WRN("No such glyph for %c", *c);
			continue;
		}
		if (!g->surface) goto advance;
		enesim_surface_size_get(g->surface, &w, &h);
		if (x >= rcoord && x <= rcoord + w)
		{
			position->glyph = g;
			position->index = idx;
			position->distance = thiz->width - rcoord;
			ret = EINA_TRUE;
			break;
		}
advance:
		rcoord += g->x_advance;
		idx--;
	}
	return ret;
}
#endif
/*----------------------------------------------------------------------------*
 *               Functions that might need to be exported on Enesim           *
 *----------------------------------------------------------------------------*/
static inline Eina_F16p16 _enesim_point_f16p16_transform(Eina_F16p16 x, Eina_F16p16 y,
		Eina_F16p16 cx, Eina_F16p16 cy, Eina_F16p16 cz)
{
	return eina_f16p16_mul(cx, x) + eina_f16p16_mul(cy, y) + cz;
}

/* FIXME store the fixed point matrix */
static inline void _renderer_affine_setup(Enesim_Renderer *r, int x, int y,
		Eina_F16p16 *fpx, Eina_F16p16 *fpy)
{
	Enesim_F16p16_Matrix matrix;
	Enesim_Matrix dmatrix;
	Eina_F16p16 xx, yy;
	Eina_F16p16 ox, oy;
	double oox, ooy;

	enesim_renderer_origin_get(r, &oox, &ooy);
	enesim_renderer_transformation_get(r, &dmatrix);
	enesim_matrix_f16p16_matrix_to(&dmatrix, &matrix);

	ox = eina_f16p16_double_from(oox);
	oy = eina_f16p16_double_from(ooy);

	xx = eina_f16p16_int_from(x);
	yy = eina_f16p16_int_from(y);

	*fpx = _enesim_point_f16p16_transform(xx, yy, matrix.xx,
			matrix.xy, matrix.xz);
	*fpy = _enesim_point_f16p16_transform(xx, yy, matrix.yx,
			matrix.yy, matrix.yz);

	*fpx = eina_f16p16_sub(*fpx, ox);
	*fpy = eina_f16p16_sub(*fpy, oy);
}
/*----------------------------------------------------------------------------*
 *                            The drawing functions                           *
 *----------------------------------------------------------------------------*/
/* This might the worst case possible, we need to fetch at every pixel what glyph we are at
 * then fetch such pixel value and finally fill the destination surface
 */
static void _enesim_renderer_text_span_draw_affine(Enesim_Renderer *r,
		int x, int y, unsigned int len, void *ddata)
{
	Enesim_Renderer_Text_Span *thiz;
	Enesim_Text_Font *font;
	Enesim_F16p16_Matrix matrix;
	Enesim_Matrix dmatrix;
	Eina_F16p16 xx, yy;
	uint32_t *dst = ddata;
	uint32_t *end = dst + len;

	/* setup the affine coordinates */
	thiz = _enesim_renderer_text_span_get(r);
	enesim_renderer_transformation_get(r, &dmatrix);
	enesim_matrix_f16p16_matrix_to(&dmatrix, &matrix);

	font = thiz->font;
	if (!font)
	{
		/* weird case ... it might be related to the multiple threads */
		return;
	}
	_renderer_affine_setup(r, x, y, &xx, &yy);
	while (dst < end)
	{
		Enesim_Text_Glyph_Position position;
		Enesim_Text_Glyph *g;
		int w, h;
		uint32_t p0 = 0;
		int sx, sy;
		uint32_t *src;
		int gx, gy;
		size_t stride;

		sy = eina_f16p16_int_to(yy);
		sx = eina_f16p16_int_to(xx);
		/* FIXME decide what to use, get() / load()? */
		if (!_enesim_renderer_text_span_get_glyph_at_ltr(thiz, font, sx, sy, &position))
		{
			goto next;
		}
		g = position.glyph;
		if (!g->surface) goto next;

		enesim_surface_size_get(g->surface, &w, &h);
		enesim_surface_data_get(g->surface, (void **)&src, &stride);
		gx = sx - position.distance;
		gy = sy - (thiz->top - g->origin);
#if 1
		p0 = argb8888_sample_good(src, stride, w, h, xx, yy, gx, gy);
#else
		/* TODO use 4 samples and generate a better looking pixel */
		if (gy < 0 || gy >= h)
			goto next;
		if (gx < 0 || gx >= w)
			goto next;
		/* get the pixel from the surface and fill from it */
		src = src + (gy * stride) + gx;
		p0 = *src;
#endif
next:
		yy += matrix.yx;
		xx += matrix.xx;
		*dst++ = p0;
	}
}

static void _enesim_renderer_text_span_draw_ltr_identity(Enesim_Renderer *r,
		int x, int y, unsigned int len, void *ddata)
{
	Enesim_Renderer_Text_Span *thiz;
	Enesim_Text_Font *font;
	Enesim_Text_Glyph_Position position;
	const char *c;
	uint32_t *dst = ddata;
	uint32_t *end = dst + len;
	double ox, oy;
	int rx;
	const char *text;
	Enesim_Color color;
	Enesim_Color fcolor;
	Enesim_Renderer *fpaint = NULL;
	uint32_t *fbuf = NULL, *buf = NULL;

	thiz = _enesim_renderer_text_span_get(r);
	enesim_renderer_origin_get(r, &ox, &oy);
	y -= oy;
	x -= ox;

	enesim_renderer_shape_fill_renderer_get(r, &fpaint);
	if (fpaint)
	{
		buf = alloca(sizeof(unsigned int) * len);
		fbuf = buf;
		enesim_renderer_sw_draw(fpaint, x, y, len, fbuf);
		enesim_renderer_unref(fpaint);
	}

	enesim_renderer_shape_fill_color_get(r, &fcolor);
	enesim_renderer_color_get(r, &color);
	if (color != 0xffffffff)
		fcolor = argb8888_mul4_sym(color, fcolor);

	font = thiz->font;
	if (!font)
	{
		/* weird case ... it might be related to the multiple threads */
		return;
	}
	/* advance to the first character that is inside the x+len span */
	if (!_enesim_renderer_text_span_get_glyph_at_ltr(thiz, font, x, y, &position))
		return;
	rx = x - position.distance;
	text = enesim_text_buffer_string_get(thiz->state.current.buffer);
	for (c = text + position.index; c && *c && dst < end; c++)
	{
		Enesim_Text_Glyph *g;
		int w, h;
		int ry;
		unsigned int rlen;
		uint32_t *mask;
		size_t stride;

		/* FIXME decide what to use, get() / load()? */
		g = enesim_text_font_glyph_load(font, *c);
		if (!g) continue;
		if (!g->surface)
			goto advance;

		enesim_surface_size_get(g->surface, &w, &h);
		enesim_surface_data_get(g->surface, (void **)&mask, &stride);
		ry = y - (thiz->top - g->origin);
		if (ry < 0 || ry >= h) goto advance;

		mask = argb8888_at(mask, stride, rx, ry);
		rlen = len < w - rx ? len : w - rx;
		if (fpaint)
		{
			if (fcolor != 0xffffffff)
				argb8888_sp_argb8888_color_argb8888_fill(dst, rlen, fbuf, fcolor, mask);
			else
				argb8888_sp_argb8888_none_argb8888_fill(dst, rlen, fbuf, 0, mask);
		}
		else
			argb8888_sp_none_color_argb8888_fill(dst, rlen, NULL, fcolor, mask);
advance:
		dst += g->x_advance - rx;
		len -= g->x_advance - rx;
		fbuf += g->x_advance - rx;
		rx = 0;
	}
}

/* FIXME this is wrong, we need to get the old font and unref the glyphs on that
 * font
 */
static Eina_Bool _enesim_renderer_text_span_calculate(Enesim_Renderer *r)
{
	Enesim_Renderer_Text_Span *thiz;
	Enesim_Text_Font *font = NULL;
	Eina_Bool invalidate;
	const char *text;
	const char *c;
	int masc;
	int mdesc;
	int len;
	char last;
	unsigned int width = 0;

	thiz = _enesim_renderer_text_span_get(r);

	/* FIXME if we do the setup/bounds/whatever that calls this
	 * function  from multiple threads the invalidate must be locked
	 */

	invalidate = _enesim_renderer_text_span_has_changed(r);
	if (!invalidate)
		return EINA_TRUE;

	text = enesim_text_buffer_string_get(thiz->state.current.buffer);
	len = enesim_text_buffer_string_length(thiz->state.current.buffer);

	font = enesim_renderer_text_span_font_get(r);
	if (!font) goto unload;

	for (c = text; *c; c++)
	{
		Enesim_Text_Glyph *g;

		g = enesim_text_font_glyph_load(font, *c);
		if (!g) continue;
		/* calculate the max len */
		width += g->x_advance;
	}
	/* remove the last advance, use only the glyph image size */
	if (len)
	{
		Enesim_Text_Glyph *g;

		last = *(text + len - 1);
		g = enesim_text_font_glyph_load(font, last);
		if (g && g->surface)
		{
			int w, h;

			enesim_surface_size_get(g->surface, &w, &h);
			width -= g->x_advance - w;
		}
	}
unload:
	/* unload old glyphs */
	if (thiz->font)
	{
		const char *old_text;

		old_text = enesim_text_buffer_string_get(thiz->state.past.buffer);
		for (c = old_text; c && *c; c++)
		{
			enesim_text_font_glyph_unload(thiz->font, *c);
		}
		enesim_text_font_unref(thiz->font);
	}
	thiz->font = font;
	enesim_renderer_text_span_max_ascent_get(r, &masc);
	enesim_renderer_text_span_max_descent_get(r, &mdesc);
	//printf("masc %d mdesc %d\n", masc, mdesc);
	thiz->width = width;
	thiz->height = masc + mdesc;
	thiz->top = masc;
	thiz->bottom = mdesc;

	/* copy current state to old state */
	enesim_text_buffer_string_set(thiz->state.past.buffer, text, len);
	return EINA_TRUE;
}
/*----------------------------------------------------------------------------*
 *                           The Text Base interface                          *
 *----------------------------------------------------------------------------*/
static const char * _enesim_renderer_text_span_name(Enesim_Renderer *r EINA_UNUSED)
{
	return "text_span";
}

static Eina_Bool _enesim_text_sw_setup(Enesim_Renderer *r,
		Enesim_Surface *s EINA_UNUSED,
		Enesim_Renderer_Sw_Fill *fill, Enesim_Error **error EINA_UNUSED)
{
	Enesim_Renderer_Text_Span *thiz;
	Enesim_Matrix_Type type;
	const char *text;

	thiz = _enesim_renderer_text_span_get(r);
	if (!thiz) return EINA_FALSE;

	text = enesim_text_buffer_string_get(thiz->state.current.buffer);
	if (!text)
	{
		DBG("No text set");
		return EINA_FALSE;
	}
	if (!_enesim_renderer_text_span_calculate(r))
		return EINA_FALSE;
	enesim_renderer_transformation_type_get(r, &type);
	switch (type)
	{
		case ENESIM_MATRIX_IDENTITY:
		*fill = _enesim_renderer_text_span_draw_ltr_identity;
		break;

		case ENESIM_MATRIX_AFFINE:
		*fill = _enesim_renderer_text_span_draw_affine;
		break;

		default:
		break;
	}

#if 0
	enesim_text_font_dump(thiz->font, "/tmp/");
#endif

	return EINA_TRUE;
}

static void _enesim_text_sw_cleanup(Enesim_Renderer *r, Enesim_Surface *s)
{
	Enesim_Renderer_Text_Span *thiz;

	thiz = _enesim_renderer_text_span_get(r);
	if (!thiz) return;
//	enesim_renderer_shape_cleanup(r, s);
}

static Eina_Bool _enesim_renderer_text_span_has_changed(Enesim_Renderer *r)
{
	Enesim_Renderer_Text_Span *thiz;

	const char *old_text;
	const char *curr_text;

	thiz = _enesim_renderer_text_span_get(r);
	/* FIXME for later */
	old_text = enesim_text_buffer_string_get(thiz->state.past.buffer);
	curr_text = enesim_text_buffer_string_get(thiz->state.current.buffer);

	if (strcmp(old_text, curr_text))
		return EINA_TRUE;
	return EINA_FALSE;
}

static void _enesim_renderer_text_span_free(Enesim_Renderer *r)
{
	Enesim_Renderer_Text_Span *thiz;

	thiz = _enesim_renderer_text_span_get(r);
	if (!thiz) return;

	if (thiz->font)
		enesim_text_font_unref(thiz->font);
	//if (thiz->state.current.str) free(thiz->state.current.str);
	//if (thiz->state.past.str) fre(thiz->state.past.str);
	free(thiz);
}

static void _enesim_renderer_text_span_bounds(Enesim_Renderer *r,
		Enesim_Rectangle *rect)
{
	Enesim_Renderer_Text_Span *thiz;
	double ox, oy;

	thiz = _enesim_renderer_text_span_get(r);
	if (!thiz) return;

	/* we should calculate the current width/height */
	_enesim_renderer_text_span_calculate(r);
	rect->x = 0;
	rect->y = 0;
	rect->w = thiz->width;
	rect->h = thiz->height;
	/* first translate */
	enesim_renderer_origin_get(r, &ox, &oy);
	rect->x += ox;
	rect->y += oy;
}

static void _enesim_renderer_text_span_destination_bounds(Enesim_Renderer *r,
		Eina_Rectangle *bounds)
{
	Enesim_Rectangle obounds;
	Enesim_Matrix_Type type;

	_enesim_renderer_text_span_bounds(r, &obounds);
	enesim_renderer_transformation_type_get(r, &type);
	/* now apply the inverse transformation */
	if (type != ENESIM_MATRIX_IDENTITY)
	{
		Enesim_Quad q;
		Enesim_Matrix m, tx;

		enesim_renderer_transformation_get(r, &tx);
		enesim_matrix_inverse(&tx, &m);
		enesim_matrix_rectangle_transform(&m, &obounds, &q);
		enesim_quad_rectangle_to(&q, &obounds);
		/* fix the antialias scaling */
		obounds.x -= m.xx;
		obounds.y -= m.yy;
		obounds.w += m.xx;
		obounds.h += m.yy;
	}
	bounds->x = floor(obounds.x);
	bounds->y = floor(obounds.y);
	bounds->w = ceil(obounds.w);
	bounds->h = ceil(obounds.h);
}

static void _enesim_renderer_text_span_features_get(Enesim_Renderer *r EINA_UNUSED,
		Enesim_Renderer_Feature *features)
{
	*features = ENESIM_RENDERER_FEATURE_TRANSLATE |
			ENESIM_RENDERER_FEATURE_ARGB8888 |
			ENESIM_RENDERER_FEATURE_AFFINE;
}

static Enesim_Renderer_Shape_Descriptor _enesim_renderer_text_span_descriptor = {
	/* .version = 			*/ ENESIM_RENDERER_API,
	/* .base_name_get = 		*/ _enesim_renderer_text_span_name,
	/* .free = 			*/ _enesim_renderer_text_span_free,
	/* .bounds_get = 		*/ _enesim_renderer_text_span_bounds,
	/* .destination_bounds_get = 	*/ _enesim_renderer_text_span_destination_bounds,
	/* .features_get = 		*/ _enesim_renderer_text_span_features_get,
	/* .is_inside = 		*/ NULL,
	/* .damage = 			*/ NULL,
	/* .has_changed = 		*/ _enesim_renderer_text_span_has_changed,
	/* .sw_hints_get = 		*/ NULL,
	/* .sw_setup = 			*/ _enesim_text_sw_setup,
	/* .sw_cleanup = 		*/ _enesim_text_sw_cleanup,
};

static Enesim_Renderer * _enesim_renderer_text_span_new(Enesim_Text_Engine *engine)
{
	Enesim_Renderer_Text_Span *thiz;
	Enesim_Renderer *r;

	thiz = calloc(1, sizeof(Enesim_Renderer_Text_Span));
	if (!thiz) return NULL;

	thiz->engine = engine;
	thiz->state.current.buffer = enesim_text_buffer_new(0);
	thiz->state.past.buffer = enesim_text_buffer_new(0);

	r = enesim_renderer_shape_new(&_enesim_renderer_text_span_descriptor, thiz);
	if (!r) goto renderer_err;

	return r;

renderer_err:
	free(thiz);
	return NULL;
}
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
EAPI Enesim_Renderer * enesim_renderer_text_span_new(void)
{
	return _enesim_renderer_text_span_new(enesim_text_engine_default_get());
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI Enesim_Renderer * enesim_renderer_text_span_new_from_engine(Enesim_Text_Engine *e)
{
	return _enesim_renderer_text_span_new(e);
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_text_span_text_set(Enesim_Renderer *r, const char *str)
{
	Enesim_Renderer_Text_Span *thiz;

	if (!str) return;
	thiz = _enesim_renderer_text_span_get(r);
	if (!thiz) return;

	enesim_text_buffer_string_set(thiz->state.current.buffer, str, -1);
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_text_span_text_get(Enesim_Renderer *r, const char **str)
{
	Enesim_Renderer_Text_Span *thiz;

	if (!str) return;
	thiz = _enesim_renderer_text_span_get(r);
	if (!thiz) return;

	*str = enesim_text_buffer_string_get(thiz->state.current.buffer);
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_text_span_direction_get(Enesim_Renderer *r, Enesim_Text_Direction *direction)
{
	Enesim_Renderer_Text_Span *thiz;

	thiz = _enesim_renderer_text_span_get(r);
	if (!thiz) return;
	*direction = thiz->direction;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_text_span_direction_set(Enesim_Renderer *r, Enesim_Text_Direction direction)
{
	Enesim_Renderer_Text_Span *thiz;

	thiz = _enesim_renderer_text_span_get(r);
	if (!thiz) return;
	thiz->direction = direction;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_text_span_buffer_get(Enesim_Renderer *r, Enesim_Text_Buffer **b)
{
	Enesim_Renderer_Text_Span *thiz;

	if (!b) return;
	thiz = _enesim_renderer_text_span_get(r);
	if (!thiz) return;
	*b = thiz->state.current.buffer;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_text_span_font_name_set(Enesim_Renderer *r, const char *font)
{
	Enesim_Renderer_Text_Span *thiz;

	if (!font) return;
	thiz = _enesim_renderer_text_span_get(r);
	if (!thiz) return;
	if (thiz->state.current.font_name) free(thiz->state.current.font_name);
	thiz->state.current.font_name = strdup(font);
	thiz->state.changed = EINA_TRUE;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_text_span_font_name_get(Enesim_Renderer *r, const char **font)
{
	Enesim_Renderer_Text_Span *thiz;

	if (!font) return;
	thiz = _enesim_renderer_text_span_get(r);
	if (!thiz) return;
	*font = thiz->state.current.font_name;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_text_span_size_set(Enesim_Renderer *r, unsigned int size)
{
	Enesim_Renderer_Text_Span *thiz;

	thiz = _enesim_renderer_text_span_get(r);
	if (!thiz) return;

	thiz->state.current.size = size;
	thiz->state.changed = EINA_TRUE;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_text_span_size_get(Enesim_Renderer *r, unsigned int *size)
{
	Enesim_Renderer_Text_Span *thiz;

	thiz = _enesim_renderer_text_span_get(r);
	if (!thiz) return;

	*size = thiz->state.current.size;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_text_span_max_ascent_get(Enesim_Renderer *r, int *masc)
{
	Enesim_Renderer_Text_Span *thiz;

	*masc = 0;
	thiz = _enesim_renderer_text_span_get(r);
	if (!thiz) return;
	_enesim_renderer_text_span_setup(r);
	if (!thiz->font) return;
	*masc = enesim_text_font_max_ascent_get(thiz->font);
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_text_span_max_descent_get(Enesim_Renderer *r, int *mdesc)
{
	Enesim_Renderer_Text_Span *thiz;

	*mdesc = 0;
	thiz = _enesim_renderer_text_span_get(r);
	if (!thiz) return;
	_enesim_renderer_text_span_setup(r);
	if (!thiz->font) return;
	*mdesc = enesim_text_font_max_descent_get(thiz->font);
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI Enesim_Text_Font * enesim_renderer_text_span_font_get(Enesim_Renderer *r)
{
	Enesim_Renderer_Text_Span *thiz;

	thiz = _enesim_renderer_text_span_get(r);
	_enesim_renderer_text_span_setup(r);

	return enesim_text_font_ref(thiz->font);
}

/**
 * TODO
 * implement it
 */
EAPI int enesim_renderer_text_span_index_at(Enesim_Renderer *r, int x, int y)
{
	/* check that x, y is inside the bounding box */
	/* get the glyph at that position */
	return 0;
}


