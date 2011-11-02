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
typedef struct _Enesim_Renderer_Shape
{
	/* properties */
	Enesim_Renderer_Shape_State current;
	Enesim_Renderer_Shape_State past;
	/* needed for the state */
	Enesim_Matrix stroke_transformation;
	Enesim_Matrix fill_transformation;
	double fill_ox, fill_oy;
	double fill_sx, fill_sy;
	/* interface */
	Enesim_Renderer_Has_Changed has_changed;
	void *data;
} Enesim_Renderer_Shape;

static inline Enesim_Renderer_Shape * _shape_get(Enesim_Renderer *r)
{
	Enesim_Renderer_Shape *thiz;

	thiz = enesim_renderer_data_get(r);
	return thiz;
}

static Eina_Bool _enesim_renderer_shape_changed(Enesim_Renderer_Shape *thiz)
{
	/* the stroke */
	/* color */
	if (thiz->current.stroke.color != thiz->past.stroke.color)
		return EINA_TRUE;
	/* weight */
	if (thiz->current.stroke.weight != thiz->past.stroke.weight)
		return EINA_TRUE;
	/* renderer */
	if (thiz->current.stroke.r != thiz->past.stroke.r)
		return EINA_TRUE;
	/* the fill */
	/* color */
	if (thiz->current.fill.color != thiz->past.fill.color)
		return EINA_TRUE;
	/* renderer */
	if (thiz->current.fill.r != thiz->past.fill.r)
		return EINA_TRUE;
	return EINA_FALSE;
}

static Eina_Bool _enesim_renderer_shape_has_changed(Enesim_Renderer *r)
{
	Enesim_Renderer_Shape *thiz;

	thiz = enesim_renderer_data_get(r);
	if (_enesim_renderer_shape_changed(thiz))
		return EINA_TRUE;
	/* call the has_changed on the descriptor */
	if (thiz->has_changed)
		return thiz->has_changed(r);
	return EINA_TRUE;
}
/*============================================================================*
 *                                 Global                                     *
 *============================================================================*/
Enesim_Renderer * enesim_renderer_shape_new(Enesim_Renderer_Descriptor *descriptor, void *data)
{
	Enesim_Renderer *r;
	Enesim_Renderer_Shape *thiz;
	Enesim_Renderer_Descriptor pdescriptor;

	thiz = calloc(1, sizeof(Enesim_Renderer_Shape));
	thiz->data = data;
	if (!thiz) return NULL;
	/* set default properties */
	thiz->current.fill.color = thiz->past.fill.color = 0xffffffff;
	thiz->current.stroke.color = thiz->past.stroke.color = 0xffffffff;
	thiz->has_changed = descriptor->has_changed;
	/* set the parent descriptor */
	pdescriptor.version = ENESIM_RENDERER_API;
	pdescriptor.name = descriptor->name;
	pdescriptor.free = descriptor->free;
	pdescriptor.boundings = descriptor->boundings;
	pdescriptor.flags = descriptor->flags;
	pdescriptor.is_inside = descriptor->is_inside;
	pdescriptor.damage = descriptor->damage;
	pdescriptor.has_changed = _enesim_renderer_shape_has_changed;
	pdescriptor.sw_setup = descriptor->sw_setup;
	pdescriptor.sw_cleanup = descriptor->sw_cleanup;
	pdescriptor.opencl_setup = descriptor->opencl_setup;
	pdescriptor.opencl_kernel_setup = descriptor->opencl_kernel_setup;
	pdescriptor.opencl_cleanup = descriptor->opencl_cleanup;

	r = enesim_renderer_new(&pdescriptor, thiz);
	return r;
}

void enesim_renderer_shape_cleanup(Enesim_Renderer *r, Enesim_Surface *s)
{
	Enesim_Renderer_Shape *thiz;

	thiz = _shape_get(r);
	if (thiz->current.fill.r &&
			(thiz->current.draw_mode & ENESIM_SHAPE_DRAW_MODE_FILL))
	{
		enesim_renderer_relative_unset(r, thiz->current.fill.r,
				&thiz->fill_transformation,
				thiz->fill_ox, thiz->fill_oy,
				thiz->fill_sx, thiz->fill_sy);
		enesim_renderer_cleanup(thiz->current.fill.r, s);
	}
}

Eina_Bool enesim_renderer_shape_setup(Enesim_Renderer *r,
		const Enesim_Renderer_State *state,
		Enesim_Surface *s,
		Enesim_Error **error)
{
	Enesim_Renderer_Shape *thiz;

	thiz = _shape_get(r);
	if (thiz->current.fill.r &&
			(thiz->current.draw_mode & ENESIM_SHAPE_DRAW_MODE_FILL))
	{
		enesim_renderer_relative_set(r, thiz->current.fill.r,
				&thiz->fill_transformation,
				&thiz->fill_ox, &thiz->fill_oy,
				&thiz->fill_sx, &thiz->fill_sy);
		if (!enesim_renderer_setup(thiz->current.fill.r, s, error))
		{
			ENESIM_RENDERER_ERROR(r, error, "Fill renderer failed");
			return EINA_FALSE;
		}
	}
	return EINA_TRUE;
}

void * enesim_renderer_shape_data_get(Enesim_Renderer *r)
{
	Enesim_Renderer_Shape *thiz;

	thiz = _shape_get(r);
	return thiz->data;
}
/*============================================================================*
 *                                   API                                      *
 *============================================================================*/
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_shape_stroke_weight_set(Enesim_Renderer *r, double weight)
{
	Enesim_Renderer_Shape *thiz;

	if (weight < 1)
		weight = 1;
	thiz = _shape_get(r);
	thiz->current.stroke.weight = weight;
}
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_shape_stroke_weight_get(Enesim_Renderer *r, double *weight)
{
	Enesim_Renderer_Shape *thiz;

	thiz = _shape_get(r);
	if (weight) *weight = thiz->current.stroke.weight;
}
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_shape_stroke_color_set(Enesim_Renderer *r, Enesim_Color color)
{
	Enesim_Renderer_Shape *thiz;

	thiz = _shape_get(r);
	thiz->current.stroke.color = color;
}
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_shape_stroke_color_get(Enesim_Renderer *r, Enesim_Color *color)
{
	Enesim_Renderer_Shape *thiz;

	thiz = _shape_get(r);
	*color = thiz->current.stroke.color;
}
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_shape_stroke_renderer_set(Enesim_Renderer *r, Enesim_Renderer *stroke)
{
	Enesim_Renderer_Shape *thiz;

	thiz = _shape_get(r);
	if (thiz->current.stroke.r)
		enesim_renderer_unref(thiz->current.stroke.r);
	thiz->current.stroke.r = stroke;
	if (thiz->current.stroke.r)
		thiz->current.stroke.r = enesim_renderer_ref(thiz->current.stroke.r);
}
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_shape_stroke_renderer_get(Enesim_Renderer *r, Enesim_Renderer **stroke)
{
	Enesim_Renderer_Shape *thiz;

	thiz = _shape_get(r);
	*stroke = thiz->current.stroke.r;
}
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_shape_fill_color_set(Enesim_Renderer *r, Enesim_Color color)
{
	Enesim_Renderer_Shape *thiz;

	thiz = _shape_get(r);
	thiz->current.fill.color = color;
}
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_shape_fill_color_get(Enesim_Renderer *r, Enesim_Color *color)
{
	Enesim_Renderer_Shape *thiz;

	thiz = _shape_get(r);
	*color = thiz->current.fill.color;
}
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_shape_fill_renderer_set(Enesim_Renderer *r, Enesim_Renderer *fill)
{
	Enesim_Renderer_Shape *thiz;

	thiz = _shape_get(r);
	if (thiz->current.fill.r)
		enesim_renderer_unref(thiz->current.fill.r);
	thiz->current.fill.r = fill;
	if (thiz->current.fill.r)
		thiz->current.fill.r = enesim_renderer_ref(thiz->current.fill.r);
}
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_shape_fill_renderer_get(Enesim_Renderer *r, Enesim_Renderer **fill)
{
	Enesim_Renderer_Shape *thiz;

	if (!fill) return;
	thiz = _shape_get(r);
	*fill = thiz->current.fill.r;
	if (thiz->current.fill.r)
		thiz->current.fill.r = enesim_renderer_ref(thiz->current.fill.r);
}
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_shape_draw_mode_set(Enesim_Renderer *r, Enesim_Shape_Draw_Mode draw_mode)
{
	Enesim_Renderer_Shape *thiz;

	thiz = _shape_get(r);
	thiz->current.draw_mode = draw_mode;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_shape_draw_mode_get(Enesim_Renderer *r, Enesim_Shape_Draw_Mode *draw_mode)
{
	Enesim_Renderer_Shape *thiz;

	thiz = _shape_get(r);
	*draw_mode = thiz->current.draw_mode;
}

