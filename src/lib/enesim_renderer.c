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
#include "libargb.h"

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
#include "enesim_renderer.h"

#include "enesim_surface_private.h"
#include "enesim_renderer_private.h"
/**
 * @todo
 * - Add a way to get/set such description
 * - Change every internal struct on Enesim to have the correct prefix
 *   looks like we are having issues with mingw
 * - We have some overlfow on the coordinates whenever we want to trasnlate or
 *   transform bounds, we need to fix the maximum and minimum for a
 *   coordinate and length
 *
 * - Maybe checking the flags for every origin/value set is too much
 * another option is to make every renderer responsable of changing the origin/scale/etc
 * if the owned renderer handles that
 */
/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/
#define ENESIM_LOG_DEFAULT enesim_log_renderer

#define ENESIM_MAGIC_RENDERER 0xe7e51420
#define ENESIM_MAGIC_CHECK_RENDERER(d)\
	do {\
		if (!EINA_MAGIC_CHECK(d, ENESIM_MAGIC_RENDERER))\
			EINA_MAGIC_FAIL(d, ENESIM_MAGIC_RENDERER);\
	} while(0)

typedef struct _Enesim_Renderer_Factory
{
	int id;
} Enesim_Renderer_Factory;

static Eina_Hash *_factories = NULL;

static void _enesim_renderer_factory_free(void *data)
{
	Enesim_Renderer_Factory *f = data;
	free(f);
}
/*----------------------------------------------------------------------------*
 *                     Functions to transform the bounds                      *
 *----------------------------------------------------------------------------*/
static inline void _enesim_renderer_bounds(Enesim_Renderer *r,
		Enesim_Rectangle *bounds)
{
	if (r->descriptor.bounds_get)
	{
		r->descriptor.bounds_get(r, bounds);
	}
	else
	{
		enesim_rectangle_coords_from(bounds, INT_MIN / 2, INT_MIN / 2, INT_MAX, INT_MAX);
	}
}

static inline void _enesim_renderer_destination_bounds(Enesim_Renderer *r,
		Eina_Rectangle *bounds)
{
	if (r->descriptor.destination_bounds_get)
	{
		r->descriptor.destination_bounds_get(r, bounds);
	}
	else
	{
		eina_rectangle_coords_from(bounds, INT_MIN / 2, INT_MIN / 2, INT_MAX, INT_MAX);
	}
}

static void _draw_internal(Enesim_Renderer *r, Enesim_Surface *s,
		Eina_Rectangle *area, int x, int y)
{
	Enesim_Backend b;

	enesim_surface_lock(s, EINA_TRUE);
	b = enesim_surface_backend_get(s);
	switch (b)
	{
		case ENESIM_BACKEND_SOFTWARE:
		enesim_renderer_sw_draw_area(r, s, area, x, y);
		break;

		case ENESIM_BACKEND_OPENCL:
#if BUILD_OPENCL
		enesim_renderer_opencl_draw(r, s, area, x, y);
#endif
		break;

		case ENESIM_BACKEND_OPENGL:
#if BUILD_OPENGL
		enesim_renderer_opengl_draw(r, s, area, x, y);
#endif
		break;

		default:
		WRN("Backend not supported %d", b);
		break;
	}
	enesim_surface_unlock(s);
}

static void _draw_list_internal(Enesim_Renderer *r, Enesim_Surface *s,
		Eina_Rectangle *area,
		Eina_List *clips, int x, int y)
{
	Enesim_Backend b;
	Eina_Rectangle *clip;
	Eina_List *l;

	enesim_surface_lock(s, EINA_TRUE);
	b = enesim_surface_backend_get(s);
	switch (b)
	{
		case ENESIM_BACKEND_SOFTWARE:
		EINA_LIST_FOREACH(clips, l, clip)
		{
			Eina_Rectangle final;

			final = *clip;
			if (!eina_rectangle_intersection(&final, area))
				continue;
			enesim_renderer_sw_draw_area(r, s, &final, x, y);
		}
		break;

		case ENESIM_BACKEND_OPENGL:
#if BUILD_OPENGL
		EINA_LIST_FOREACH(clips, l, clip)
		{
			Eina_Rectangle final;

			final = *clip;
			if (!eina_rectangle_intersection(&final, area))
				continue;
			enesim_renderer_opengl_draw(r, s, &final, x, y);
		}
#endif
		break;

		default:
		WRN("Backend not supported %d", b);
		break;
	}
	enesim_surface_unlock(s);
}

static inline void _surface_bounds(Enesim_Surface *s, Eina_Rectangle *bounds)
{
	bounds->x = 0;
	bounds->y = 0;
	enesim_surface_size_get(s, &bounds->w, &bounds->h);
}

static void _enesim_renderer_factory_setup(Enesim_Renderer *r)
{
	Enesim_Renderer_Factory *f;
	char renderer_name[PATH_MAX];
	const char *descriptor_name = NULL;

	if (!_factories) return;
	if (r->descriptor.base_name_get)
		descriptor_name = r->descriptor.base_name_get(r);
	if (!descriptor_name)
		descriptor_name = "unknown";
	f = eina_hash_find(_factories, descriptor_name);
	if (!f)
	{
		f = calloc(1, sizeof(Enesim_Renderer_Factory));
		eina_hash_add(_factories, descriptor_name, f);
	}
	/* assign a new name for it automatically */
	snprintf(renderer_name, PATH_MAX, "%s%d", descriptor_name, f->id++);
	enesim_renderer_name_set(r, renderer_name);
}

/*============================================================================*
 *                                 Global                                     *
 *============================================================================*/
void enesim_renderer_init(void)
{
	_factories = eina_hash_string_superfast_new(
			_enesim_renderer_factory_free);
	enesim_renderer_sw_init();
#if BUILD_OPENGL
	enesim_renderer_opengl_init();
#endif
}

void enesim_renderer_shutdown(void)
{
	eina_hash_free(_factories);
	_factories = NULL;
	enesim_renderer_sw_shutdown();
#if BUILD_OPENGL
	enesim_renderer_opengl_shutdown();
#endif
}

/* FIXME export this */
void * enesim_renderer_backend_data_get(Enesim_Renderer *r, Enesim_Backend b)
{
	return r->backend_data[b];
}

void enesim_renderer_backend_data_set(Enesim_Renderer *r, Enesim_Backend b, void *data)
{
	r->backend_data[b] = data;
}

Eina_Bool enesim_renderer_state_transformation_set(Enesim_Renderer_State2 *thiz,
		const Enesim_Matrix *m)
{
	if (!m)
	{
		enesim_matrix_identity(&thiz->current.transformation);
		thiz->current.transformation_type = ENESIM_MATRIX_IDENTITY;
		return EINA_TRUE;
	}
	thiz->current.transformation = *m;
	thiz->changed = EINA_TRUE;
	return EINA_TRUE;
}

Eina_Bool enesim_renderer_state_transformation_get(Enesim_Renderer_State2 *thiz,
		Enesim_Matrix *m)
{
	if (!m) return EINA_FALSE;
	*m = thiz->current.transformation;
	return EINA_TRUE;
}


void enesim_renderer_state_name_set(Enesim_Renderer_State2 *thiz, const char *name)
{
	if (thiz->name) free(thiz->name);
	thiz->name = strdup(name);
}

const char * enesim_renderer_state_name_get(Enesim_Renderer_State2 *thiz)
{
	return thiz->name;
}

void enesim_renderer_state_rop_set(Enesim_Renderer_State2 *thiz,
		Enesim_Rop rop)
{
	thiz->current.rop = rop;
	thiz->changed = EINA_TRUE;
}

Eina_Bool enesim_renderer_state_rop_get(Enesim_Renderer_State2 *thiz,
		Enesim_Rop *rop)
{
	if (!rop) return EINA_FALSE;
	*rop = thiz->current.rop;
	return EINA_TRUE;
}

void enesim_renderer_state_visibility_set(Enesim_Renderer_State2 *thiz,
		Eina_Bool visibility)
{
	thiz->current.visibility = visibility;
	thiz->changed = EINA_TRUE;
}

Eina_Bool enesim_renderer_state_visibility_get(Enesim_Renderer_State2 *thiz,
		Eina_Bool *visibility)
{
	if (!visibility) return EINA_FALSE;
	*visibility = thiz->current.visibility;
	return EINA_TRUE;
}

void enesim_renderer_state_color_set(Enesim_Renderer_State2 *thiz,
		Enesim_Color color)
{
	thiz->current.color = color;
	thiz->changed = EINA_TRUE;
}

Eina_Bool enesim_renderer_state_color_get(Enesim_Renderer_State2 *thiz,
		Enesim_Color *color)
{
	if (!color) return EINA_FALSE;
	*color = thiz->current.color;
	return EINA_TRUE;
}

void enesim_renderer_state_x_set(Enesim_Renderer_State2 *thiz, double x)
{
	thiz->current.ox = x;
	thiz->changed = EINA_TRUE;
}

Eina_Bool enesim_renderer_state_x_get(Enesim_Renderer_State2 *thiz, double *x)
{
	if (!x) return EINA_FALSE;
	*x = thiz->current.ox;
	return EINA_FALSE;
}

void enesim_renderer_state_y_set(Enesim_Renderer_State2 *thiz, double y)
{
	thiz->current.oy = y;
	thiz->changed = EINA_TRUE;
}

Eina_Bool enesim_renderer_state_y_get(Enesim_Renderer_State2 *thiz, double *y)
{
	if (!y) return EINA_FALSE;
	*y = thiz->current.oy;
	return EINA_FALSE;
}

void enesim_renderer_state_commit(Enesim_Renderer_State2 *thiz)
{
	Enesim_Renderer *old_mask;

	/* keep the referenceable objects */
	old_mask = thiz->past.mask;
	/* swap the state */
	thiz->past = thiz->current;
	/* increment the referenceable objects */
	if (thiz->past.mask)
		enesim_renderer_ref(thiz->past.mask);
	/* release the referenceable objects */
	if (old_mask)
		enesim_renderer_unref(old_mask);
}

Eina_Bool enesim_renderer_state_changed(Enesim_Renderer_State2 *thiz)
{
	if (!thiz->changed)
		return EINA_FALSE;
	/* the visibility */
	if (thiz->current.visibility != thiz->past.visibility)
	{
		return EINA_TRUE;
	}
	/* the rop */
	if (thiz->current.rop != thiz->past.rop)
	{
		return EINA_TRUE;
	}
	/* the color */
	if (thiz->current.color != thiz->past.color)
	{
		return EINA_TRUE;
	}
	/* the origin */
	if (thiz->current.ox != thiz->past.ox || thiz->current.oy != thiz->past.oy)
	{
		return EINA_TRUE;
	}
	/* the transformation */
	if (thiz->current.transformation_type != thiz->past.transformation_type)
	{
		return EINA_TRUE;
	}

	if (!enesim_matrix_is_equal(&thiz->current.transformation, &thiz->past.transformation))
	{
		return EINA_TRUE;
	}
	/* the mask should be the last as it implies a renderer change */
	if (thiz->current.mask && !thiz->past.mask)
		return EINA_TRUE;
	if (!thiz->current.mask && thiz->past.mask)
		return EINA_TRUE;
	if (thiz->current.mask)
	{
		if (enesim_renderer_has_changed(thiz->current.mask))
		{
			return EINA_TRUE;
		}
	}

	return EINA_FALSE;
}

void enesim_renderer_state_clear(Enesim_Renderer_State2 *thiz)
{
	/* past */
	if (thiz->past.mask)
	{
		enesim_renderer_unref(thiz->past.mask);
		thiz->past.mask = NULL;
	}
	/* current */
	if (thiz->current.mask)
	{
		enesim_renderer_unref(thiz->current.mask);
		thiz->current.mask = NULL;
	}

	if (thiz->name)
	{
		free(thiz->name);
		thiz->name = NULL;
	}
}

void enesim_renderer_state_init(Enesim_Renderer_State2 *thiz)
{
	/* common properties */
	thiz->current.visibility = thiz->past.visibility = EINA_TRUE;
	thiz->current.ox = thiz->past.ox = 0;
	thiz->current.oy = thiz->past.oy = 0;
	thiz->current.color = thiz->past.color = ENESIM_COLOR_FULL;
	thiz->current.rop = thiz->past.rop = ENESIM_FILL;
	enesim_matrix_identity(&thiz->current.transformation);
	enesim_matrix_identity(&thiz->past.transformation);
	thiz->current.transformation_type = ENESIM_MATRIX_IDENTITY;
	thiz->past.transformation_type = ENESIM_MATRIX_IDENTITY;
}

/*============================================================================*
 *                                   API                                      *
 *============================================================================*/
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI Enesim_Renderer * enesim_renderer_new(Enesim_Renderer_Descriptor
		*descriptor, void *data)
{
	Enesim_Renderer *r;

	if (!descriptor) return NULL;
	if (descriptor->version > ENESIM_RENDERER_API) {
		ERR("API version %d is greater than %d", descriptor->version, ENESIM_RENDERER_API);
		return NULL;
	}

	r = calloc(1, sizeof(Enesim_Renderer));
	/* first check the passed in functions */
	if (!descriptor->is_inside) WRN("No is_inside() function available");
	if (!descriptor->bounds_get) WRN("No bounding() function available");
	if (!descriptor->flags_get) WRN("No flags() function available");
	if (!descriptor->sw_setup) WRN("No sw_setup() function available");
	if (!descriptor->sw_cleanup) WRN("No sw_cleanup() function available");
	if (!descriptor->free) WRN("No free() function available");
	r->descriptor = *descriptor;
	r->data = data;
	/* now initialize the renderer common properties */
	EINA_MAGIC_SET(r, ENESIM_MAGIC_RENDERER);
	/* private stuff */
	r->current_flags = 0;
	enesim_rectangle_coords_from(&r->past_bounds, INT_MIN / 2, INT_MIN / 2, INT_MAX, INT_MAX);
	eina_rectangle_coords_from(&r->past_destination_bounds, INT_MIN / 2, INT_MIN / 2, INT_MAX, INT_MAX);
	r->prv_data = eina_hash_string_superfast_new(NULL);
	eina_lock_new(&r->lock);
	/* always set the first reference */
	r = enesim_renderer_ref(r);
	_enesim_renderer_factory_setup(r);

	return r;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void * enesim_renderer_data_get(Enesim_Renderer *r)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);

	return r->data;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI Enesim_Renderer * enesim_renderer_ref(Enesim_Renderer *r)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	r->ref++;
	return r;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_unref(Enesim_Renderer *r)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	r->ref--;
	if (!r->ref)
	{
		if (r->descriptor.free)
			r->descriptor.free(r);
		eina_lock_free(&r->lock);

		eina_hash_free(r->prv_data);
		/* remove all the private data */
		enesim_renderer_sw_free(r);
#if BUILD_OPENGL
		enesim_renderer_opengl_free(r);
#endif
#if BUILD_OPENCL
		enesim_renderer_opencl_free(r);
#endif
		/* finally */
		free(r);
	}
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI int enesim_renderer_ref_count(Enesim_Renderer *r)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	return r->ref;
}


/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_lock(Enesim_Renderer *r)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	eina_lock_take(&r->lock);
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_unlock(Enesim_Renderer *r)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	eina_lock_release(&r->lock);
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI Eina_Bool enesim_renderer_setup(Enesim_Renderer *r, Enesim_Surface *s, Enesim_Error **error)
{
	Enesim_Backend b;
	Eina_Bool ret = EINA_TRUE;
	const char *name;

	ENESIM_MAGIC_CHECK_RENDERER(r);
	enesim_renderer_name_get(r, &name);
	DBG("Setting up the renderer '%s'", name);
	if (r->in_setup)
	{
		WRN("Renderer '%s' already in the setup process", name);
		return EINA_TRUE;
	}
	enesim_renderer_lock(r);

	b = enesim_surface_backend_get(s);
	switch (b)
	{
		case ENESIM_BACKEND_SOFTWARE:
		if (!enesim_renderer_sw_setup(r, s, error))
		{
			ENESIM_RENDERER_ERROR(r, error, "Software setup failed on '%s'", name);
			ret = EINA_FALSE;
			break;
		}
		break;

		case ENESIM_BACKEND_OPENCL:
#if BUILD_OPENCL
		if (!enesim_renderer_opencl_setup(r, s, error))
		{
			ENESIM_RENDERER_ERROR(r, error, "OpenCL setup failed");
			ret = EINA_FALSE;
		}
#endif
		break;

		case ENESIM_BACKEND_OPENGL:
#if BUILD_OPENGL
		if (!enesim_renderer_opengl_setup(r, s, error))
		{
			ENESIM_RENDERER_ERROR(r, error, "OpenGL setup failed");
			ret = EINA_FALSE;
		}
#endif
		break;

		default:
		break;
	}
	r->in_setup = EINA_TRUE;
	if (ret)
	{
		/* given that we already did the setup, the current and previous should be equal
		 * when calculating the bounds
		 */
		_enesim_renderer_bounds(r, &r->current_bounds);
		_enesim_renderer_destination_bounds(r, &r->current_destination_bounds);
		enesim_renderer_flags(r, &r->current_flags);
	}

	return ret;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_cleanup(Enesim_Renderer *r, Enesim_Surface *s)
{
	Enesim_Backend b;
	const char *name;

	ENESIM_MAGIC_CHECK_RENDERER(r);
	enesim_renderer_name_get(r, &name);
	DBG("Cleaning up the renderer '%s'", name);
	if (!r->in_setup)
	{
		WRN("Renderer '%s' has not done the setup first", name);
		return;
	}

	b = enesim_surface_backend_get(s);
	switch (b)
	{
		case ENESIM_BACKEND_SOFTWARE:
		enesim_renderer_sw_cleanup(r, s);
		break;

		case ENESIM_BACKEND_OPENGL:
#if BUILD_OPENGL
		enesim_renderer_opengl_cleanup(r, s);
#endif
		break;

		default:
		break;
	}
	r->past_bounds = r->current_bounds;
	r->past_destination_bounds = r->current_destination_bounds;
	r->in_setup = EINA_FALSE;

	enesim_renderer_unlock(r);
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_flags(Enesim_Renderer *r, Enesim_Renderer_Flag *flags)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);

	if (!flags) return;
	*flags = 0;
	if (r->in_setup)
	{
		*flags = r->current_flags;
		return;
	}
	if (r->descriptor.flags_get)
	{
		r->descriptor.flags_get(r, flags);
		return;
	}
}
/**
 * Sets the transformation matrix of a renderer
 * @param[in] r The renderer to set the transformation matrix on
 * @param[in] m The transformation matrix to set
 */
EAPI void enesim_renderer_transformation_set(Enesim_Renderer *r, const Enesim_Matrix *m)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);

	if (r->descriptor.transformation_set)
		r->descriptor.transformation_set(r, m);
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_transformation_get(Enesim_Renderer *r, Enesim_Matrix *m)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	if (r->descriptor.transformation_get)
		r->descriptor.transformation_get(r, m);
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_name_set(Enesim_Renderer *r, const char *name)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	if (r->descriptor.name_set)
		r->descriptor.name_set(r, name);
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_name_get(Enesim_Renderer *r, const char **name)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	if (!name) return;
	if (r->descriptor.name_get)
		*name = r->descriptor.name_get(r);
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_origin_set(Enesim_Renderer *r, double x, double y)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	enesim_renderer_x_origin_set(r, x);
	enesim_renderer_y_origin_set(r, y);
}
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_origin_get(Enesim_Renderer *r, double *x, double *y)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	enesim_renderer_x_origin_get(r, x);
	enesim_renderer_y_origin_get(r, y);
}

/**
 * To  be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_x_origin_set(Enesim_Renderer *r, double x)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	if (r->descriptor.origin_x_set)
		r->descriptor.origin_x_set(r, x);
}

/**
 * To  be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_x_origin_get(Enesim_Renderer *r, double *x)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	if (x && r->descriptor.origin_x_get)
		*x = r->descriptor.origin_x_get(r);
}

/**
 * To  be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_y_origin_set(Enesim_Renderer *r, double y)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	if (r->descriptor.origin_y_set)
		r->descriptor.origin_y_set(r, y);
}

/**
 * To  be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_y_origin_get(Enesim_Renderer *r, double *y)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	if (y && r->descriptor.origin_y_get)
		*y = r->descriptor.origin_y_get(r);
}

/**
 * To  be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_color_set(Enesim_Renderer *r, Enesim_Color color)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	if (r->descriptor.color_set)
		r->descriptor.color_set(r, color);
}
/**
 * To  be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_color_get(Enesim_Renderer *r, Enesim_Color *color)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	if (color)
	{
		if (r->descriptor.color_set)
			*color = r->descriptor.color_get(r);
		else
			*color = ENESIM_COLOR_FULL;
	}
}
/**
 * To  be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_visibility_set(Enesim_Renderer *r, Eina_Bool visible)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	if (r->descriptor.visibility_set)
		r->descriptor.visibility_set(r, visible);
}
/**
 * To  be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_visibility_get(Enesim_Renderer *r, Eina_Bool *visible)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	if (visible && r->descriptor.visibility_get)
		*visible = r->descriptor.visibility_get(r);
}

/**
 * Gets the bounding box of the renderer on its own coordinate space without
 * adding the origin translation
 * @param[in] r The renderer to get the bounds from
 * @param[out] rect The rectangle to store the bounds
 */
EAPI void enesim_renderer_bounds(Enesim_Renderer *r, Enesim_Rectangle *rect)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	if (!rect) return;
	if (r->in_setup)
	{
		*rect = r->current_bounds;
	}
	else
	{
		_enesim_renderer_bounds(r, rect);
	}
}

/**
 * Gets the bounding box of the renderer on its own coordinate space without
 * adding the origin translation
 * @param[in] r The renderer to get the bounds from
 * @param[out] prev The rectangle to store the previous bounds
 * @param[out] curr The rectangle to store the current bounds
 */
EAPI void enesim_renderer_bounds_extended(Enesim_Renderer *r, Enesim_Rectangle *prev, Enesim_Rectangle *curr)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);

	if (curr)
		_enesim_renderer_bounds(r, curr);
	if (prev)
		*prev = r->past_bounds;
}

/**
 * To  be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_destination_bounds(Enesim_Renderer *r, Eina_Rectangle *rect,
		int x, int y)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);

	if (!rect) return;
	if (r->in_setup)
	{
		*rect = r->current_destination_bounds;
		return;
	}
	else
	{
		_enesim_renderer_destination_bounds(r, rect);
	}
	if (rect->x != INT_MIN / 2)
		rect->x -= x;
	if (rect->y != INT_MIN / 2)
		rect->y -= y;
}

/**
 * To  be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_destination_bounds_extended(Enesim_Renderer *r,
		Eina_Rectangle *prev, Eina_Rectangle *curr,
		int x, int y)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);

	if (curr)
	{
		enesim_renderer_destination_bounds(r, curr, x, y);
	}
	if (prev)
	{
		*prev = r->past_destination_bounds;
		if (prev->x != INT_MIN / 2)
			prev->x -= x;
		if (prev->y != INT_MIN / 2)
			prev->y -= y;
	}
}

/**
 * To  be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_rop_set(Enesim_Renderer *r, Enesim_Rop rop)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	if (r->descriptor.rop_set)
		r->descriptor.rop_set(r, rop);
}

/**
 * To  be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_rop_get(Enesim_Renderer *r, Enesim_Rop *rop)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	if (rop)
	{
		if (r->descriptor.rop_get)
			*rop = r->descriptor.rop_get(r);
		else
			*rop = ENESIM_FILL;
	}
}

/**
 * To  be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_mask_set(Enesim_Renderer *r, Enesim_Renderer *mask)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	if (r->descriptor.mask_set)
		r->descriptor.mask_set(r, mask);
	else
		enesim_renderer_unref(mask);
}

/**
 * To  be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_mask_get(Enesim_Renderer *r, Enesim_Renderer **mask)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	if (mask && r->descriptor.mask_get)
		*mask = r->descriptor.mask_get(r);
}

/**
 * To  be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_hints_get(Enesim_Renderer *r, Enesim_Renderer_Hint *hints)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	if (!hints) return;
	if (r->descriptor.hints_get)
		r->descriptor.hints_get(r, hints);
	else
		*hints = 0;
}

/**
 * To  be documented
 * FIXME: To be fixed
 */
EAPI Eina_Bool enesim_renderer_is_inside(Enesim_Renderer *r, double x, double y)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	if (r->descriptor.is_inside) return r->descriptor.is_inside(r, x, y);
	return EINA_TRUE;
}

/**
 * To  be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_private_set(Enesim_Renderer *r, const char *name, void *data)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	eina_hash_add(r->prv_data, name, data);
}

/**
 * To  be documented
 * FIXME: To be fixed
 */
EAPI void * enesim_renderer_private_get(Enesim_Renderer *r, const char *name)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	return eina_hash_find(r->prv_data, name);
}

/**
 * Draw a renderer into a surface
 * @param[in] r The renderer to draw
 * @param[in] s The surface to draw the renderer into
 * @param[in] clip The area on the destination surface to limit the drawing
 * @param[in] x The x origin of the destination surface
 * @param[in] y The y origin of the destination surface
 * TODO What about the mask?
 */
EAPI Eina_Bool enesim_renderer_draw(Enesim_Renderer *r, Enesim_Surface *s,
		Eina_Rectangle *clip, int x, int y, Enesim_Error **error)
{
	Eina_Rectangle final;
	Eina_Bool ret = EINA_FALSE;

	ENESIM_MAGIC_CHECK_RENDERER(r);
	ENESIM_MAGIC_CHECK_SURFACE(s);

	if (!enesim_renderer_setup(r, s, error))
		goto end;

	if (!clip)
	{
		_surface_bounds(s, &final);
	}
	else
	{
		Eina_Rectangle surface_size;

		final.x = clip->x;
		final.y = clip->y;
		final.w = clip->w;
		final.h = clip->h;
		_surface_bounds(s, &surface_size);
		if (!eina_rectangle_intersection(&final, &surface_size))
		{
			WRN("The renderer %p bounds does not intersect with the surface", r);
			goto end;
		}
	}
	/* clip against the destination rectangle */
	if (!eina_rectangle_intersection(&final, &r->current_destination_bounds))
	{
		WRN("The renderer %p bounds does not intersect on the destination rectangle", r);
		goto end;
	}
	_draw_internal(r, s, &final, x, y);
	ret = EINA_TRUE;

	/* TODO set the format again */
end:
	enesim_renderer_cleanup(r, s);

	return ret;
}

/**
 * Draw a renderer into a surface
 * @param[in] r The renderer to draw
 * @param[in] s The surface to draw the renderer into
 * @param[in] clips A list of clipping areas on the destination surface to limit the drawing
 * @param[in] x The x origin of the destination surface
 * @param[in] y The y origin of the destination surface
 */
EAPI Eina_Bool enesim_renderer_draw_list(Enesim_Renderer *r, Enesim_Surface *s,
		Eina_List *clips, int x, int y, Enesim_Error **error)
{
	Eina_Rectangle surface_size;
	Eina_Bool ret = EINA_FALSE;

	if (!clips)
	{
		enesim_renderer_draw(r, s, NULL, x, y, error);
		return EINA_TRUE;
	}

	ENESIM_MAGIC_CHECK_RENDERER(r);
	ENESIM_MAGIC_CHECK_SURFACE(s);

	/* setup the common parameters */
	if (!enesim_renderer_setup(r, s, error))
		goto end;

	_surface_bounds(s, &surface_size);
	/* clip against the destination rectangle */
	if (!eina_rectangle_intersection(&r->current_destination_bounds, &surface_size))
	{
		WRN("The renderer %p bounds does not intersect on the destination rectangle", r);
		goto end;
	}
	_draw_list_internal(r, s, &r->current_destination_bounds, clips, x, y);
	ret = EINA_TRUE;
	/* TODO set the format again */
end:
	enesim_renderer_cleanup(r, s);

	return ret;
}

/**
 * To  be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_error_add(Enesim_Renderer *r, Enesim_Error **error, const char *file,
		const char *function, int line, char *fmt, ...)
{
	const char *name;
	va_list args;
	char str[PATH_MAX];
	int num;

	ENESIM_MAGIC_CHECK_RENDERER(r);
	if (!error)
		return;
	enesim_renderer_name_get(r, &name);
	va_start(args, fmt);
	num = snprintf(str, PATH_MAX, "%s:%d %s %s ", file, line, function, name);
	num += vsnprintf(str + num, PATH_MAX - num, fmt, args);
	str[num++] = '\n';
	str[num] = '\0';
	va_end(args);

	*error = enesim_error_add(*error, str);
}

#if 0
/**
 * To  be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_change_notify(Enesim_Renderer *r)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	/* if we have notified a changed already dont do it again */
	if (r->notified_change) return;

	r->notified_change = TRUE;
	/* inform about the change */
}

EAPI Enesim_Renderer_Listener * enesim_renderer_change_listen(Enesim_Renderer *r,
		Enesim_Renderer_Change_Cb cb, void *data)
{
	/* register on the list of listener */
}

EAPI void enesim_renderer_change_mute(Enesim_Renderer *r,
		Enesim_Renderer_Listener *listener)
{
	/* unregister from the list of listener */

}

EAPI void enesim_renderer_change_mute_full(Enesim_Renderer *r,
		Enesim_Renderer_Change_Cb cb, void *data)
{
	/* unregister from the list of listener */
	/* call the other function */
}


#endif

/**
 * To  be documented
 * FIXME: To be fixed
 */
EAPI Eina_Bool enesim_renderer_has_changed(Enesim_Renderer *r)
{
	Eina_Bool ret = EINA_TRUE;

	ENESIM_MAGIC_CHECK_RENDERER(r);
#if 0
	/* in case the renderer has not notified about a change just return
	 * false
	 */
	if (!r->notifed_change) return EINA_FALSE;
	/* TODO split the below code into an internal version */
	/* TODO modify the logic a bit */
#endif

	/* if the renderer does not has the descriptor
	 * function we assume it is always true */
	if (r->descriptor.has_changed)
	{
		ret = r->descriptor.has_changed(r);
	}
	else
	{
		const char *name;
		enesim_renderer_name_get(r, &name);
		WRN("The renderer '%s' does not implement the change callback", name);
	}

	if (ret)
	{
		const char *name;
		enesim_renderer_name_get(r, &name);
		DBG("The renderer '%s' has changed", name);
	}
	return ret;
}

/**
 * To  be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_damages_get(Enesim_Renderer *r, Enesim_Renderer_Damage_Cb cb, void *data)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);

	if (!cb) return;
#if 0
	/* TODO change the API to return TRUE/FALSE whenever there are damages or not */
	/* in case the renderer has not notified about a change just return */
	if (!r->notifed_change) return;
	/* use the internal has changed function */
#endif

	if (r->descriptor.damages_get)
	{
		r->descriptor.damages_get(r, &r->past_destination_bounds, cb,
				data);
	}
	else
	{
		Eina_Rectangle current_bounds;

		if (!enesim_renderer_has_changed(r))
			return;

		/* send the old bounds and the new one */
		enesim_renderer_destination_bounds(r, &current_bounds, 0, 0);
		cb(r, &current_bounds, EINA_FALSE, data);
		cb(r, &r->past_destination_bounds, EINA_TRUE, data);
	}
}
