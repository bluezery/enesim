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
#define ENESIM_RENDERER_PROXY_MAGIC_CHECK(d) \
	do {\
		if (!EINA_MAGIC_CHECK(d, ENESIM_RENDERER_PROXY_MAGIC))\
			EINA_MAGIC_FAIL(d, ENESIM_RENDERER_PROXY_MAGIC);\
	} while(0)

typedef struct _Enesim_Renderer_Proxy {
	EINA_MAGIC
	/* the properties */
	Enesim_Renderer *proxied;
	/* generated at state setup */
	Enesim_Renderer_Sw_Fill proxied_fill;
} Enesim_Renderer_Proxy;

static inline Enesim_Renderer_Proxy * _proxy_get(Enesim_Renderer *r)
{
	Enesim_Renderer_Proxy *thiz;

	thiz = enesim_renderer_data_get(r);
	ENESIM_RENDERER_PROXY_MAGIC_CHECK(thiz);

	return thiz;
}

static void _proxy_span(Enesim_Renderer *r,
		const Enesim_Renderer_State *state,
		int x, int y,
		unsigned int len, void *dst)
{
	Enesim_Renderer_Proxy *thiz;

 	thiz = _proxy_get(r);
	enesim_renderer_sw_draw(thiz->proxied, x, y, len, dst);
}
/*----------------------------------------------------------------------------*
 *                      The Enesim's renderer interface                       *
 *----------------------------------------------------------------------------*/
static const char * _proxy_name(Enesim_Renderer *r)
{
	return "proxy";
}

static Eina_Bool _proxy_state_setup(Enesim_Renderer *r,
		const Enesim_Renderer_State *states[ENESIM_RENDERER_STATES],
		Enesim_Surface *s,
		Enesim_Renderer_Sw_Fill *fill, Enesim_Error **error)
{
	Enesim_Renderer_Proxy *thiz;

 	thiz = _proxy_get(r);
	if (!thiz->proxied)
	{
		ENESIM_RENDERER_ERROR(r, error, "No proxied");
		return EINA_FALSE;
	}
	if (!enesim_renderer_setup(thiz->proxied, s, error))
	{
		const char *name;

		enesim_renderer_name_get(thiz->proxied, &name);
		ENESIM_RENDERER_ERROR(r, error, "Proxy renderer %s can not setup", name);
		return EINA_FALSE;
	}
	*fill = _proxy_span;
	return EINA_TRUE;
}

static void _proxy_state_cleanup(Enesim_Renderer *r, Enesim_Surface *s)
{
	Enesim_Renderer_Proxy *thiz;

 	thiz = _proxy_get(r);
	if (!thiz->proxied) return;
	enesim_renderer_cleanup(thiz->proxied, s);
}

static void _proxy_flags(Enesim_Renderer *r, const Enesim_Renderer_State *state,
		Enesim_Renderer_Flag *flags)
{
	Enesim_Renderer_Proxy *thiz;

	thiz = _proxy_get(r);
	*flags = 0;
}

static void _proxy_hints(Enesim_Renderer *r, const Enesim_Renderer_State *state,
		Enesim_Renderer_Hint *hints)
{
	Enesim_Renderer_Proxy *thiz;

	thiz = _proxy_get(r);
	*hints = 0;
	if (!thiz->proxied)
		return;
	enesim_renderer_hints_get(thiz->proxied, hints);
}

static void _proxy_boundings(Enesim_Renderer *r,
		const Enesim_Renderer_State *states[ENESIM_RENDERER_STATES],
		Enesim_Rectangle *rect)
{
	Enesim_Renderer_Proxy *thiz;

	thiz = _proxy_get(r);
	if (!thiz->proxied)
	{
		rect->x = 0;
		rect->y = 0;
		rect->w = 0;
		rect->h = 0;
		return;
	}
	enesim_renderer_boundings(thiz->proxied, rect);
}

static void _proxy_destination_boundings(Enesim_Renderer *r,
		const Enesim_Renderer_State *states[ENESIM_RENDERER_STATES],
		Eina_Rectangle *boundings)
{
	Enesim_Renderer_Proxy *thiz;

	thiz = _proxy_get(r);
	if (!thiz->proxied)
	{
		boundings->x = 0;
		boundings->y = 0;
		boundings->w = 0;
		boundings->h = 0;
		return;
	}
	enesim_renderer_destination_boundings(thiz->proxied, boundings, 0, 0);
}

static Eina_Bool _proxy_has_changed(Enesim_Renderer *r,
		const Enesim_Renderer_State *states[ENESIM_RENDERER_STATES])
{
	Enesim_Renderer_Proxy *thiz;
	Eina_Bool ret = EINA_FALSE;

	thiz = _proxy_get(r);
	if (thiz->proxied)
		ret = enesim_renderer_has_changed(thiz->proxied);
	return ret;
}

static void _proxy_damage(Enesim_Renderer *r,
		const Eina_Rectangle *old_boundings,
		const Enesim_Renderer_State *states[ENESIM_RENDERER_STATES],
		Enesim_Renderer_Damage_Cb cb, void *data)
{
	Enesim_Renderer_Proxy *thiz;

	thiz = _proxy_get(r);
	if (!thiz->proxied)
		return;
	enesim_renderer_damages_get(thiz->proxied,
			cb, data);
}

static void _proxy_free(Enesim_Renderer *r)
{
	Enesim_Renderer_Proxy *thiz;

	thiz = _proxy_get(r);
	if (thiz->proxied)
		enesim_renderer_unref(thiz->proxied);
	free(thiz);
}

static Enesim_Renderer_Descriptor _descriptor = {
	/* .version = 			*/ ENESIM_RENDERER_API,
	/* .name = 			*/ _proxy_name,
	/* .free = 			*/ _proxy_free,
	/* .boundings = 		*/ _proxy_boundings,
	/* .destination_boundings =	*/ _proxy_destination_boundings,
	/* .flags = 			*/ _proxy_flags,
	/* .hints_get = 		*/ _proxy_hints,
	/* .is_inside = 		*/ NULL,
	/* .damage = 			*/ _proxy_damage,
	/* .has_changed = 		*/ _proxy_has_changed,
	/* .sw_setup = 			*/ _proxy_state_setup,
	/* .sw_cleanup = 		*/ _proxy_state_cleanup,
	/* .opencl_setup =		*/ NULL,
	/* .opencl_kernel_setup =	*/ NULL,
	/* .opencl_cleanup =		*/ NULL,
	/* .opengl_setup =          	*/ NULL,
	/* .opengl_shader_setup = 	*/ NULL,
	/* .opengl_cleanup =        	*/ NULL
};
/*============================================================================*
 *                                   API                                      *
 *============================================================================*/
/**
 * Creates a clipper renderer
 * @return The new renderer
 */
EAPI Enesim_Renderer * esvg_renderer_proxy_new(void)
{
	Enesim_Renderer *r;
	Enesim_Renderer_Proxy *thiz;

	thiz = calloc(1, sizeof(Enesim_Renderer_Proxy));
	if (!thiz) return NULL;
	EINA_MAGIC_SET(thiz, ENESIM_RENDERER_PROXY_MAGIC);
	r = enesim_renderer_new(&_descriptor, thiz);
	return r;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void esvg_renderer_proxy_proxied_set(Enesim_Renderer *r,
		Enesim_Renderer *proxied)
{
	Enesim_Renderer_Proxy *thiz;

	thiz = _proxy_get(r);
	if (thiz->proxied)
		enesim_renderer_unref(thiz->proxied);
	thiz->proxied = proxied;
	if (thiz->proxied)
		thiz->proxied = enesim_renderer_ref(thiz->proxied);
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void esvg_renderer_proxy_proxied_get(Enesim_Renderer *r,
		Enesim_Renderer **proxied)
{
	Enesim_Renderer_Proxy *thiz;

	thiz = _proxy_get(r);
	if (!proxied) return;
	*proxied = thiz->proxied;
	if (thiz->proxied)
		thiz->proxied = enesim_renderer_ref(thiz->proxied);
}
