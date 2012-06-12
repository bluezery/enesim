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
#define ENESIM_RENDERER_CLIPPER_MAGIC_CHECK(d) \
	do {\
		if (!EINA_MAGIC_CHECK(d, ENESIM_RENDERER_CLIPPER_MAGIC))\
			EINA_MAGIC_FAIL(d, ENESIM_RENDERER_CLIPPER_MAGIC);\
	} while(0)

typedef struct _Enesim_Renderer_Clipper_Damage_Data
{
	Eina_Rectangle *boundings;
	Enesim_Renderer_Damage_Cb real_cb;
	void *real_data;
} Enesim_Renderer_Clipper_Damage_Data;

typedef struct _Enesim_Renderer_Clipper_State
{
	Enesim_Renderer *content;
	double width;
	double height;
} Enesim_Renderer_Clipper_State;

typedef struct _Enesim_Renderer_Clipper {
	EINA_MAGIC
	/* the properties */
	Enesim_Renderer_Clipper_State current;
	Enesim_Renderer_Clipper_State past;
	/* generated at state setup */
	Enesim_Renderer_Sw_Fill content_fill;
	/* private */
	Eina_Bool changed : 1;
} Enesim_Renderer_Clipper;

static inline Enesim_Renderer_Clipper * _clipper_get(Enesim_Renderer *r)
{
	Enesim_Renderer_Clipper *thiz;

	thiz = enesim_renderer_data_get(r);
	ENESIM_RENDERER_CLIPPER_MAGIC_CHECK(thiz);

	return thiz;
}

static void _clipper_span(Enesim_Renderer *r,
		const Enesim_Renderer_State *state,
		int x, int y,
		unsigned int len, void *dst)
{
	Enesim_Renderer_Clipper *thiz;

 	thiz = _clipper_get(r);
	enesim_renderer_sw_draw(thiz->current.content, x, y, len, dst);
}

/* called from the optimized case of the damages to just clip the damages
 * to our own bounds
 */
static Eina_Bool _clipper_damage_cb(Enesim_Renderer *r,
		Eina_Rectangle *area, Eina_Bool past, void *data)
{
	Enesim_Renderer_Clipper_Damage_Data *ddata = data;
	Eina_Rectangle new_area = *area;

	/* here we just intersect the damages with our bounds */
	if (eina_rectangle_intersection(&new_area, ddata->boundings))
		ddata->real_cb(r, &new_area, past, ddata->real_data);
	return EINA_TRUE;
}

static Eina_Bool _clipper_changed_basic(Enesim_Renderer_Clipper *thiz)
{
	if (!thiz->changed) return EINA_FALSE;

	if (thiz->current.width != thiz->past.width)
		return EINA_TRUE;

	if (thiz->current.height != thiz->past.height)
		return EINA_TRUE;

	if (thiz->current.content != thiz->past.content)
		return EINA_TRUE;
	return EINA_FALSE;
}
/*----------------------------------------------------------------------------*
 *                      The Enesim's renderer interface                       *
 *----------------------------------------------------------------------------*/
static const char * _clipper_name(Enesim_Renderer *r)
{
	return "clipper";
}

static Eina_Bool _clipper_state_setup(Enesim_Renderer *r,
		const Enesim_Renderer_State *states[ENESIM_RENDERER_STATES],
		Enesim_Surface *s,
		Enesim_Renderer_Sw_Fill *fill, Enesim_Error **error)
{
	Enesim_Renderer_Clipper *thiz;

 	thiz = _clipper_get(r);
	if (!thiz->current.content)
	{
		ENESIM_RENDERER_ERROR(r, error, "No content");
		return EINA_FALSE;
	}
	if (!enesim_renderer_setup(thiz->current.content, s, error))
	{
		const char *name;

		enesim_renderer_name_get(thiz->current.content, &name);
		ENESIM_RENDERER_ERROR(r, error, "Content renderer %s can not setup", name);
		return EINA_FALSE;
	}
	*fill = _clipper_span;
	return EINA_TRUE;
}

static void _clipper_state_cleanup(Enesim_Renderer *r, Enesim_Surface *s)
{
	Enesim_Renderer_Clipper *thiz;

 	thiz = _clipper_get(r);
	thiz->changed = EINA_FALSE;
	thiz->past = thiz->current;

	if (!thiz->current.content) return;
	enesim_renderer_cleanup(thiz->current.content, s);
}

static void _clipper_flags(Enesim_Renderer *r, const Enesim_Renderer_State *state,
		Enesim_Renderer_Flag *flags)
{
	Enesim_Renderer_Clipper *thiz;

	thiz = _clipper_get(r);
	*flags = ENESIM_RENDERER_FLAG_TRANSLATE;
}

static void _clipper_hints(Enesim_Renderer *r, const Enesim_Renderer_State *state,
		Enesim_Renderer_Hint *hints)
{
	Enesim_Renderer_Clipper *thiz;

	thiz = _clipper_get(r);
	*hints = 0;
	if (thiz->current.content)
	{
		Enesim_Renderer_Hint content_hints;
		Enesim_Rop rop;
		Enesim_Color color;

		enesim_renderer_hints_get(thiz->current.content, &content_hints);
		enesim_renderer_rop_get(thiz->current.content, &rop);
		if (rop == state->rop || (content_hints & ENESIM_RENDERER_HINT_ROP))
			*hints |= ENESIM_RENDERER_HINT_ROP;
		enesim_renderer_color_get(thiz->current.content, &color);
		if (color == state->color || (content_hints & ENESIM_RENDERER_HINT_COLORIZE))
			*hints |= ENESIM_RENDERER_HINT_COLORIZE;
	}
}

static void _clipper_boundings(Enesim_Renderer *r,
		const Enesim_Renderer_State *states[ENESIM_RENDERER_STATES],
		Enesim_Rectangle *rect)
{
	Enesim_Renderer_Clipper *thiz;
	const Enesim_Renderer_State *cs = states[ENESIM_STATE_CURRENT];

	thiz = _clipper_get(r);
	rect->x = cs->ox;
	rect->y = cs->oy;
	rect->w = thiz->current.width;
	rect->h = thiz->current.height;
}

static void _clipper_destination_boundings(Enesim_Renderer *r,
		const Enesim_Renderer_State *states[ENESIM_RENDERER_STATES],
		Eina_Rectangle *boundings)
{
	Enesim_Rectangle oboundings;

	_clipper_boundings(r, states, &oboundings);
	boundings->x = floor(oboundings.x);
	boundings->y = floor(oboundings.y);
	boundings->w = ceil(oboundings.x - boundings->x + oboundings.w) + 1;
	boundings->h = ceil(oboundings.y - boundings->y + oboundings.h) + 1;
}

static Eina_Bool _clipper_has_changed(Enesim_Renderer *r,
		const Enesim_Renderer_State *states[ENESIM_RENDERER_STATES])
{
	Enesim_Renderer_Clipper *thiz;
	Eina_Bool ret = EINA_FALSE;

	thiz = _clipper_get(r);
	if (thiz->current.content)
	{
		ret = enesim_renderer_has_changed(thiz->current.content);
		if (ret) goto end;
	}

	ret = _clipper_changed_basic(thiz);
end:
	return ret;
}

static void _clipper_damage(Enesim_Renderer *r,
		const Eina_Rectangle *old_boundings,
		const Enesim_Renderer_State *states[ENESIM_RENDERER_STATES],
		Enesim_Renderer_Damage_Cb cb, void *data)
{
	Enesim_Renderer_Clipper *thiz;
	Eina_Rectangle current_boundings;

	thiz = _clipper_get(r);

	/* get the current boundings */
	enesim_renderer_destination_boundings(r, &current_boundings, 0, 0);
	/* if we have changed then send the old and the current */
	/* FIXME we use the origin but dont take care of the origin property here */
	if (_clipper_changed_basic(thiz))
	{
		cb(r, old_boundings, EINA_TRUE, data);
		cb(r, &current_boundings, EINA_FALSE, data);
	}
	/* if not, send the content only */
	else
	{
		Enesim_Renderer_Clipper_Damage_Data ddata;

		if (!thiz->current.content) return;
		ddata.real_cb = cb;
		ddata.real_data = data;
		ddata.boundings = &current_boundings;

		enesim_renderer_damages_get(thiz->current.content, _clipper_damage_cb, &ddata);
	}
}

static void _clipper_free(Enesim_Renderer *r)
{
	Enesim_Renderer_Clipper *thiz;

	thiz = _clipper_get(r);
	if (thiz->current.content)
		enesim_renderer_unref(thiz->current.content);
	free(thiz);
}

static Enesim_Renderer_Descriptor _descriptor = {
	/* .version = 			*/ ENESIM_RENDERER_API,
	/* .name = 			*/ _clipper_name,
	/* .free = 			*/ _clipper_free,
	/* .boundings = 		*/ _clipper_boundings,
	/* .destination_boundings =	*/ _clipper_destination_boundings,
	/* .flags = 			*/ _clipper_flags,
	/* .hints_get = 		*/ _clipper_hints,
	/* .is_inside = 		*/ NULL,
	/* .damage = 			*/ _clipper_damage,
	/* .has_changed = 		*/ _clipper_has_changed,
	/* .sw_setup = 			*/ _clipper_state_setup,
	/* .sw_cleanup = 		*/ _clipper_state_cleanup,
	/* .opencl_setup =		*/ NULL,
	/* .opencl_kernel_setup =	*/ NULL,
	/* .opencl_cleanup =		*/ NULL,
	/* .opengl_setup =          	*/ NULL,
	/* .opengl_cleanup =        	*/ NULL
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
	EINA_MAGIC_SET(thiz, ENESIM_RENDERER_CLIPPER_MAGIC);
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
	if (thiz->current.content) enesim_renderer_unref(thiz->current.content);
	thiz->current.content = content;
	if (thiz->current.content)
		thiz->current.content = enesim_renderer_ref(thiz->current.content);
	thiz->changed = EINA_TRUE;
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
	*content = thiz->current.content;
	if (thiz->current.content)
		thiz->current.content = enesim_renderer_ref(thiz->current.content);
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
	thiz->current.width = width;
	thiz->changed = EINA_TRUE;
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
	*width = thiz->current.width;
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
	thiz->current.height = height;
	thiz->changed = EINA_TRUE;
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
	*height = thiz->current.height;
}

