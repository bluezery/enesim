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
typedef struct _Background Enesim_Renderer_Clipper;

struct _Background {
	/* the properties */
	Enesim_Renderer *content;
	double width;
	double height;
	/* the content properties */
	Enesim_Rop old_rop;
	Enesim_Matrix old_matrix;
	double old_ox;
	double old_oy;
	double old_sx;
	double old_sy;
	/* generated at state setup */
	Enesim_Renderer_Sw_Fill content_fill;
};

static inline Enesim_Renderer_Clipper * _clipper_get(Enesim_Renderer *r)
{
	Enesim_Renderer_Clipper *thiz;

	thiz = enesim_renderer_data_get(r);
	return thiz;
}

static void _span(Enesim_Renderer *r, int x, int y,
		unsigned int len, void *dst)
{
	Enesim_Renderer_Clipper *thiz;

 	thiz = _clipper_get(r);
	thiz->content_fill(thiz->content, x, y, len, dst);
}

static void _content_cleanup(Enesim_Renderer_Clipper *thiz, Enesim_Surface *s)
{
	Enesim_Renderer_Flag flags;

	enesim_renderer_flags(thiz->content, &flags);
	if (flags & ENESIM_RENDERER_FLAG_TRANSLATE)
	{
		enesim_renderer_origin_set(thiz->content, thiz->old_ox, thiz->old_oy);
	}
	if (flags & ENESIM_RENDERER_FLAG_SCALE)
	{
		enesim_renderer_scale_set(thiz->content, thiz->old_sx, thiz->old_sy);
	}
	if (flags & (ENESIM_RENDERER_FLAG_AFFINE | ENESIM_RENDERER_FLAG_PROJECTIVE))
	{
		enesim_renderer_transformation_set(thiz->content, &thiz->old_matrix);
	}
	enesim_renderer_sw_cleanup(thiz->content, s);
	/* FIXME add the rop */
}
/*----------------------------------------------------------------------------*
 *                      The Enesim's renderer interface                       *
 *----------------------------------------------------------------------------*/
static const char * _clipper_name(Enesim_Renderer *r)
{
	return "clipper";
}

static Eina_Bool _clipper_state_setup(Enesim_Renderer *r,
		const Enesim_Renderer_State *state,
		Enesim_Surface *s,
		Enesim_Renderer_Sw_Fill *fill, Enesim_Error **error)
{
	Enesim_Renderer_Clipper *thiz;
	Enesim_Matrix matrix;
	Enesim_Renderer_Flag flags;

 	thiz = _clipper_get(r);
	if (!thiz->content)
	{
		printf("no content\n");
		return EINA_FALSE;
	}
	enesim_renderer_flags(thiz->content, &flags);
	if (flags & ENESIM_RENDERER_FLAG_TRANSLATE)
	{
		double ox, oy;

		enesim_renderer_origin_get(r, &ox, &oy);
		enesim_renderer_origin_get(thiz->content, &thiz->old_ox, &thiz->old_oy);
		enesim_renderer_origin_set(thiz->content, ox, oy);
	}
	if (flags & ENESIM_RENDERER_FLAG_SCALE)
	{
		double sx, sy;

		enesim_renderer_scale_get(r, &sx, &sy);
		enesim_renderer_scale_get(thiz->content, &thiz->old_sx, &thiz->old_sy);
		enesim_renderer_scale_set(thiz->content, sx, sy);
	}
	if (flags & (ENESIM_RENDERER_FLAG_AFFINE | ENESIM_RENDERER_FLAG_PROJECTIVE))
	{
		enesim_renderer_transformation_get(r, &matrix);
		enesim_renderer_transformation_get(thiz->content, &thiz->old_matrix);
		enesim_renderer_transformation_set(thiz->content, &matrix);
	}
	/* FIXME add the rop */
	if (!enesim_renderer_sw_setup(thiz->content, state, s, error))
	{
		printf("content cannot setup\n");
		/* restore the values */
		_content_cleanup(thiz, s);
		return EINA_FALSE;
	}
	*fill = _span;
	thiz->content_fill = enesim_renderer_sw_fill_get(thiz->content);
	return EINA_TRUE;
}

static void _clipper_state_cleanup(Enesim_Renderer *r, Enesim_Surface *s)
{
	Enesim_Renderer_Clipper *thiz;

 	thiz = _clipper_get(r);
	if (!thiz->content) return;

	_content_cleanup(thiz, s);
}


static void _clipper_flags(Enesim_Renderer *r, Enesim_Renderer_Flag *flags)
{
	Enesim_Renderer_Clipper *thiz;

	thiz = _clipper_get(r);
	if (!thiz || !thiz->content)
	{
		*flags = 0;
		return;
	}

	enesim_renderer_flags(thiz->content, flags);
}

static void _clipper_boundings(Enesim_Renderer *r, Enesim_Rectangle *rect)
{
	Enesim_Renderer_Clipper *thiz;

	thiz = _clipper_get(r);
	rect->x = 0;
	rect->y = 0;
	rect->w = thiz->width;
	rect->h = thiz->height;
}

static void _clipper_free(Enesim_Renderer *r)
{
	Enesim_Renderer_Clipper *thiz;

	thiz = _clipper_get(r);
	if (thiz->content)
		enesim_renderer_unref(thiz->content);
	free(thiz);
}

static Enesim_Renderer_Descriptor _descriptor = {
	/* .version =    */ ENESIM_RENDERER_API,
	/* .name =       */ _clipper_name,
	/* .free =       */ _clipper_free,
	/* .boundings =  */ _clipper_boundings,
	/* .flags =      */ _clipper_flags,
	/* .is_inside =  */ NULL,
	/* .damage =     */ NULL,
	/* .sw_setup =   */ _clipper_state_setup,
	/* .sw_cleanup = */ _clipper_state_cleanup
};
/*============================================================================*
 *                                   API                                      *
 *============================================================================*/
/**
 * Creates a clipper renderer
 * @return The new renderer
 */
EAPI Enesim_Renderer * enesim_renderer_clipper_new(void)
{
	Enesim_Renderer *r;
	Enesim_Renderer_Clipper *thiz;

	thiz = calloc(1, sizeof(Enesim_Renderer_Clipper));
	if (!thiz) return NULL;
	r = enesim_renderer_new(&_descriptor, thiz);
	return r;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_clipper_content_set(Enesim_Renderer *r,
		Enesim_Renderer *content)
{
	Enesim_Renderer_Clipper *thiz;

	thiz = _clipper_get(r);
	if (thiz->content) enesim_renderer_unref(thiz->content);
	thiz->content = content;
	if (thiz->content)
		thiz->content = enesim_renderer_ref(thiz->content);
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_clipper_content_get(Enesim_Renderer *r,
		Enesim_Renderer **content)
{
	Enesim_Renderer_Clipper *thiz;

	thiz = _clipper_get(r);
	if (!content) return;
	*content = thiz->content;
	if (thiz->content)
		thiz->content = enesim_renderer_ref(thiz->content);
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_clipper_width_set(Enesim_Renderer *r,
		double width)
{
	Enesim_Renderer_Clipper *thiz;

	thiz = _clipper_get(r);
	thiz->width = width;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_clipper_width_get(Enesim_Renderer *r,
		double *width)
{
	Enesim_Renderer_Clipper *thiz;

	thiz = _clipper_get(r);
	*width = thiz->width;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_clipper_height_set(Enesim_Renderer *r,
		double height)
{
	Enesim_Renderer_Clipper *thiz;

	thiz = _clipper_get(r);
	thiz->height = height;

}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_clipper_height_get(Enesim_Renderer *r,
		double *height)
{
	Enesim_Renderer_Clipper *thiz;

	thiz = _clipper_get(r);
	*height = thiz->height;
}

