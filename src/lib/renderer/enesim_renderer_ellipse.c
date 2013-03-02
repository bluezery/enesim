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
#include "enesim_error.h"
#include "enesim_color.h"
#include "enesim_rectangle.h"
#include "enesim_matrix.h"
#include "enesim_pool.h"
#include "enesim_buffer.h"
#include "enesim_surface.h"
#include "enesim_compositor.h"
#include "enesim_renderer.h"
#include "enesim_renderer_shape.h"
#include "enesim_renderer_path.h"
#include "enesim_renderer_ellipse.h"

#include "enesim_renderer_private.h"
#include "enesim_renderer_shape_private.h"
#include "enesim_renderer_shape_path_private.h"
/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/
#define ENESIM_RENDERER_ELLIPSE_MAGIC_CHECK(d) \
	do {\
		if (!EINA_MAGIC_CHECK(d, ENESIM_RENDERER_ELLIPSE_MAGIC))\
			EINA_MAGIC_FAIL(d, ENESIM_RENDERER_ELLIPSE_MAGIC);\
	} while(0)

typedef struct _Enesim_Renderer_Ellipse_State {
	double x;
	double y;
	double rx;
	double ry;
} Enesim_Renderer_Ellipse_State;

typedef struct _Enesim_Renderer_Ellipse
{
	EINA_MAGIC
	/* properties */
	Enesim_Renderer_Ellipse_State current;
	Enesim_Renderer_Ellipse_State past;
	/* private */
	Eina_Bool changed : 1;
	Eina_Bool generated : 1;
} Enesim_Renderer_Ellipse;

static inline Enesim_Renderer_Ellipse * _ellipse_get(Enesim_Renderer *r)
{
	Enesim_Renderer_Ellipse *thiz;

	thiz = enesim_renderer_shape_path_data_get(r);
	ENESIM_RENDERER_ELLIPSE_MAGIC_CHECK(thiz);

	return thiz;
}

static Eina_Bool _ellipse_properties_have_changed(Enesim_Renderer_Ellipse *thiz)
{
	if (!thiz->changed) return EINA_FALSE;
	/* the rx */
	if (thiz->current.rx != thiz->past.rx)
		return EINA_TRUE;
	/* the ry */
	if (thiz->current.ry != thiz->past.ry)
		return EINA_TRUE;
	/* the x */
	if (thiz->current.x != thiz->past.x)
		return EINA_TRUE;
	/* the y */
	if (thiz->current.y != thiz->past.y)
		return EINA_TRUE;
	return EINA_FALSE;
}

static void _ellipse_get_real(Enesim_Renderer_Ellipse *thiz,
		Enesim_Renderer *r,
		double *x, double *y, double *rx, double *ry)
{
	Enesim_Shape_Draw_Mode draw_mode;

	*rx = thiz->current.rx;
	*ry = thiz->current.ry;
	*x = thiz->current.x;
	*y = thiz->current.y;

	enesim_renderer_shape_draw_mode_get(r, &draw_mode);
	if (draw_mode & ENESIM_SHAPE_DRAW_MODE_STROKE)
	{
		Enesim_Shape_Stroke_Location location;
		double sw;

		enesim_renderer_shape_stroke_weight_get(r, &sw);
		enesim_renderer_shape_stroke_location_get(r, &location);
		switch (location)
		{
			case ENESIM_SHAPE_STROKE_OUTSIDE:
			*rx += sw / 2.0;
			*ry += sw / 2.0;
			break;

			case ENESIM_SHAPE_STROKE_INSIDE:
			*rx -= sw / 2.0;
			*ry -= sw / 2.0;
			break;

			case ENESIM_SHAPE_STROKE_CENTER:
			break;
		}
	}
}
/*----------------------------------------------------------------------------*
 *                      The Enesim's renderer interface                       *
 *----------------------------------------------------------------------------*/
static const char * _ellipse_base_name_get(Enesim_Renderer *r EINA_UNUSED)
{
	return "ellipse";
}

static Eina_Bool _ellipse_setup(Enesim_Renderer *r, Enesim_Renderer *path,
		Enesim_Error **error)
{
	Enesim_Renderer_Ellipse *thiz;
	double rx, ry;
	double x, y;

	thiz = _ellipse_get(r);
	_ellipse_get_real(thiz, r, &x, &y, &rx, &ry);
	if (!thiz || (thiz->current.rx <= 0) || (thiz->current.ry <= 0))
	{
		ENESIM_RENDERER_ERROR(r, error, "Wrong size %gx%g",
				thiz->current.rx, thiz->current.ry);
		return EINA_FALSE;
	}

	if (_ellipse_properties_have_changed(thiz) && !thiz->generated)
	{
		enesim_renderer_path_command_clear(path);
		enesim_renderer_path_move_to(path, x, y - ry);
		enesim_renderer_path_arc_to(path, rx, ry, 0, EINA_FALSE, EINA_TRUE, x + rx, y);
		enesim_renderer_path_arc_to(path, rx, ry, 0, EINA_FALSE, EINA_TRUE, x, y + ry);
		enesim_renderer_path_arc_to(path, rx, ry, 0, EINA_FALSE, EINA_TRUE, x - rx, y);
		enesim_renderer_path_arc_to(path, rx, ry, 0, EINA_FALSE, EINA_TRUE, x, y - ry);
	}
	return EINA_TRUE;
}

static void _ellipse_cleanup(Enesim_Renderer *r)
{
	Enesim_Renderer_Ellipse *thiz;

	thiz = _ellipse_get(r);
	thiz->past = thiz->current;
	thiz->changed = EINA_FALSE;
}

static void _ellipse_bounds_get(Enesim_Renderer *r,
		Enesim_Rectangle *rect)
{
	Enesim_Renderer_Ellipse *thiz;
	Enesim_Shape_Draw_Mode draw_mode;
	Enesim_Matrix_Type type;
	double sw = 0;

	thiz = _ellipse_get(r);
	enesim_renderer_shape_draw_mode_get(r, &draw_mode);
	if (draw_mode & ENESIM_SHAPE_DRAW_MODE_STROKE)
	{
		Enesim_Shape_Stroke_Location location;
		enesim_renderer_shape_stroke_weight_get(r, &sw);
		enesim_renderer_shape_stroke_location_get(r, &location);
		switch (location)
		{
			case ENESIM_SHAPE_STROKE_CENTER:
			sw /= 2.0;
			break;

			case ENESIM_SHAPE_STROKE_INSIDE:
			sw = 0.0;
			break;

			case ENESIM_SHAPE_STROKE_OUTSIDE:
			break;
		}
	}
	rect->x = thiz->current.x - thiz->current.rx;
	rect->y = thiz->current.y - thiz->current.ry;
	rect->w = (thiz->current.rx + sw) * 2;
	rect->h = (thiz->current.ry + sw) * 2;

	/* apply the geometry transformation */
	enesim_renderer_transformation_type_get(r, &type);
	if (type != ENESIM_MATRIX_IDENTITY)
	{
		Enesim_Matrix m;
		Enesim_Quad q;

		enesim_renderer_transformation_get(r, &m);
		enesim_matrix_rectangle_transform(&m, rect, &q);
		enesim_quad_rectangle_to(&q, rect);
	}
}

static Eina_Bool _ellipse_has_changed(Enesim_Renderer *r)
{
	Enesim_Renderer_Ellipse *thiz;

	thiz = _ellipse_get(r);
	return _ellipse_properties_have_changed(thiz);
}

static void _ellipse_free(Enesim_Renderer *r)
{
	Enesim_Renderer_Ellipse *thiz;

	thiz = _ellipse_get(r);
	free(thiz);
}

static void _ellipse_shape_features_get(Enesim_Renderer *r EINA_UNUSED, Enesim_Shape_Feature *features)
{
	*features = ENESIM_SHAPE_FLAG_FILL_RENDERER |
			ENESIM_SHAPE_FLAG_STROKE_RENDERER |
			ENESIM_SHAPE_FLAG_STROKE_LOCATION;
}

static Enesim_Renderer_Shape_Path_Descriptor _ellipse_descriptor = {
	/* .base_name_get = 		*/ _ellipse_base_name_get,
	/* .free = 			*/ _ellipse_free,
	/* .has_changed = 		*/ _ellipse_has_changed,
	/* .shape_features_get =	*/ _ellipse_shape_features_get,
	/* .bounds_get = 			*/ _ellipse_bounds_get,
	/* .setup = 			*/ _ellipse_setup,
	/* .cleanup = 			*/ _ellipse_cleanup,
};
/*============================================================================*
 *                                 Global                                     *
 *============================================================================*/
/*============================================================================*
 *                                   API                                      *
 *============================================================================*/
/**
 * @brief Create a new ellipse renderer.
 *
 * @return A new ellipse renderer.
 *
 * This function returns a newly allocated ellipse renderer. On memory
 * error, this function returns @c NULL.
 */
EAPI Enesim_Renderer * enesim_renderer_ellipse_new(void)
{
	Enesim_Renderer *r;
	Enesim_Renderer_Ellipse *thiz;

	thiz = calloc(1, sizeof(Enesim_Renderer_Ellipse));
	if (!thiz) return NULL;
	EINA_MAGIC_SET(thiz, ENESIM_RENDERER_ELLIPSE_MAGIC);
	r = enesim_renderer_shape_path_new(&_ellipse_descriptor, thiz);
	/* to maintain compatibility */
	enesim_renderer_shape_stroke_location_set(r, ENESIM_SHAPE_STROKE_INSIDE);
	return r;
}
/**
 * @brief Set the coordinates of the center of a ellipse renderer.
 *
 * @param[in] r The ellipse renderer.
 * @param[in] x The X coordinate of the center.
 * @param[in] y The Y coordinate of the center.
 *
 * This function sets the coordinates of the center of the ellipse
 * renderer @p r to the values @p x and @p y.
 */
EAPI void enesim_renderer_ellipse_center_set(Enesim_Renderer *r, double x, double y)
{
	Enesim_Renderer_Ellipse *thiz;

	thiz = _ellipse_get(r);
	if (!thiz) return;
	if ((thiz->current.x == x) && (thiz->current.y == y))
		return;
	thiz->current.x = x;
	thiz->current.y = y;
	thiz->changed = EINA_TRUE;
	thiz->generated = EINA_FALSE;
}
/**
 * @brief Retrieve the coordinates of the center of a ellipse renderer.
 *
 * @param[in] r The ellipse renderer.
 * @param[out] x The X coordinate of the center.
 * @param[out] y The Y coordinate of the center.
 *
 * This function stores the coordinates value of the center of
 * the ellipse renderer @p r in the buffers @p x and @p y. These buffers
 * can be @c NULL.
 */
EAPI void enesim_renderer_ellipse_center_get(Enesim_Renderer *r, double *x, double *y)
{
	Enesim_Renderer_Ellipse *thiz;

	thiz = _ellipse_get(r);
	if (x) *x = thiz->current.x;
	if (y) *y = thiz->current.y;
}

/**
 * @brief Set the radii of a ellipse renderer.
 *
 * @param[in] r The ellipse renderer.
 * @param[in] radius_x The radius along the X axis.
 * @param[in] radius_y The radius along the Y axis.
 *
 * This function sets the radii of the ellipse renderer @p r to the
 * values @p radius_x along the X axis and @p radius_y along the Y axis.
 */
EAPI void enesim_renderer_ellipse_radii_set(Enesim_Renderer *r, double radius_x, double radius_y)
{
	Enesim_Renderer_Ellipse *thiz;

	thiz = _ellipse_get(r);
	if (!thiz) return;
	if ((thiz->current.rx == radius_x) && (thiz->current.ry == radius_y))
		return;
	thiz->current.rx = radius_x;
	thiz->current.ry = radius_y;
	thiz->changed = EINA_TRUE;
	thiz->generated = EINA_FALSE;
}

/**
 * @brief Retrieve the radii of a ellipse renderer.
 *
 * @param[in] r The ellipse renderer.
 * @param[out] radius_x The radius along the X axis.
 * @param[out] radius_y The radius along the Y axis.
 *
 * This function stores the radiis of the ellipse renderer @p r in the
 * buffers @p radius_x for the radius along the X axis and @p radius_y
 * for the radius along the Y axis. These buffers can be @c NULL.
 */
EAPI void enesim_renderer_ellipse_radii_get(Enesim_Renderer *r, double *radius_x, double *radius_y)
{
	Enesim_Renderer_Ellipse *thiz;

	thiz = _ellipse_get(r);
	if (radius_x) *radius_x = thiz->current.rx;
	if (radius_y) *radius_y = thiz->current.ry;
}

/**
 * @brief Set the X coordinate of the center of a ellipse renderer.
 *
 * @param[in] r The ellipse renderer.
 * @param[in] x The X coordinate.
 *
 * This function sets the X coordinate of the center of the ellipse
 * renderer @p r to the value @p x.
 */
EAPI void enesim_renderer_ellipse_x_set(Enesim_Renderer *r, double x)
{
	Enesim_Renderer_Ellipse *thiz;
	thiz = _ellipse_get(r);
	thiz->current.x = x;
	thiz->changed = EINA_TRUE;
	thiz->generated = EINA_FALSE;
}

/**
 * @brief Set the Y coordinate of the center of a ellipse renderer.
 *
 * @param[in] r The ellipse renderer.
 * @param[in] y The Y coordinate.
 *
 * This function sets the Y coordinate of the center of the ellipse
 * renderer @p r to the value @p y.
 */
EAPI void enesim_renderer_ellipse_y_set(Enesim_Renderer *r, double y)
{
	Enesim_Renderer_Ellipse *thiz;
	thiz = _ellipse_get(r);
	thiz->current.y = y;
	thiz->changed = EINA_TRUE;
	thiz->generated = EINA_FALSE;
}

/**
 * @brief Set the radius along the X axis of a ellipse renderer.
 *
 * @param[in] r The ellipse renderer.
 * @param[in] rx The radius along the X axis.
 *
 * This function sets the radius along the X axis of the ellipse
 * renderer @p r to the value @p rx.
 */
EAPI void enesim_renderer_ellipse_x_radius_set(Enesim_Renderer *r, double rx)
{
	Enesim_Renderer_Ellipse *thiz;
	thiz = _ellipse_get(r);
	thiz->current.rx = rx;
	thiz->changed = EINA_TRUE;
	thiz->generated = EINA_FALSE;
}

/**
 * @brief Set the radius along the Y axis of a ellipse renderer.
 *
 * @param[in] r The ellipse renderer.
 * @param[in] ry The radius along the Y axis.
 *
 * This function sets the radius along the Y axis of the ellipse
 * renderer @p r to the value @p ry.
 */
EAPI void enesim_renderer_ellipse_y_radius_set(Enesim_Renderer *r, double ry)
{
	Enesim_Renderer_Ellipse *thiz;
	thiz = _ellipse_get(r);
	thiz->current.ry = ry;
	thiz->changed = EINA_TRUE;
	thiz->generated = EINA_FALSE;
}
