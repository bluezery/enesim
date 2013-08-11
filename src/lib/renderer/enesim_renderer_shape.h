/* ENESIM - Drawing Library
 * Copyright (C) 2007-2013 Jorge Luis Zapata
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
#ifndef ENESIM_RENDERER_SHAPE_H_
#define ENESIM_RENDERER_SHAPE_H_

/**
 * @defgroup Enesim_Renderer_Shape_Group Shapes
 * @brief Geometric shape renderer
 * @ingroup Enesim_Renderer_Group
 * @{
 */

typedef enum _Enesim_Renderer_Shape_Feature
{
	ENESIM_RENDERER_SHAPE_FEATURE_FILL_RENDERER 	= (1 << 0),
	ENESIM_RENDERER_SHAPE_FEATURE_STROKE_RENDERER	= (1 << 1),
	ENESIM_RENDERER_SHAPE_FEATURE_STROKE_LOCATION	= (1 << 2),
	ENESIM_RENDERER_SHAPE_FEATURE_STROKE_DASH	= (1 << 3),
} Enesim_Renderer_Shape_Feature;

typedef enum _Enesim_Renderer_Shape_Draw_Mode
{
	ENESIM_RENDERER_SHAPE_DRAW_MODE_FILL 	= (1 << 0),
	ENESIM_RENDERER_SHAPE_DRAW_MODE_STROKE	= (1 << 1),
} Enesim_Renderer_Shape_Draw_Mode;

typedef enum _Enesim_Renderer_Shape_Stroke_Location
{
	ENESIM_RENDERER_SHAPE_STROKE_LOCATION_INSIDE,
	ENESIM_RENDERER_SHAPE_STROKE_LOCATION_OUTSIDE,
	ENESIM_RENDERER_SHAPE_STROKE_LOCATION_CENTER,
} Enesim_Renderer_Shape_Stroke_Location;

typedef struct _Enesim_Renderer_Shape_Stroke_Dash
{
	double length;
	double gap;
} Enesim_Renderer_Shape_Stroke_Dash;

#define ENESIM_RENDERER_SHAPE_DRAW_MODE_STROKE_FILL (ENESIM_RENDERER_SHAPE_DRAW_MODE_FILL | ENESIM_RENDERER_SHAPE_DRAW_MODE_STROKE)

typedef enum _Enesim_Renderer_Shape_Stroke_Cap
{
	ENESIM_RENDERER_SHAPE_STROKE_CAP_BUTT,
	ENESIM_RENDERER_SHAPE_STROKE_CAP_ROUND,
	ENESIM_RENDERER_SHAPE_STROKE_CAP_SQUARE,
	ENESIM_RENDERER_SHAPE_STROKE_CAPS,
} Enesim_Renderer_Shape_Stroke_Cap;

typedef enum _Enesim_Renderer_Shape_Stroke_Join
{
	ENESIM_RENDERER_SHAPE_STROKE_JOIN_MITER,
	ENESIM_RENDERER_SHAPE_STROKE_JOIN_ROUND,
	ENESIM_RENDERER_SHAPE_STROKE_JOIN_BEVEL,
	ENESIM_RENDERER_SHAPE_STROKE_JOINS,
} Enesim_Renderer_Shape_Stroke_Join;

typedef enum _Enesim_Renderer_Shape_Fill_Rule
{
	ENESIM_RENDERER_SHAPE_FILL_RULE_NON_ZERO,
	ENESIM_RENDERER_SHAPE_FILL_RULE_EVEN_ODD,
	ENESIM_RENDERER_SHAPE_FILL_RULES,
} Enesim_Renderer_Shape_Fill_Rule;

EAPI void enesim_renderer_shape_features_get(Enesim_Renderer *r, Enesim_Renderer_Shape_Feature *features);

/* stroke properties */
EAPI void enesim_renderer_shape_stroke_color_set(Enesim_Renderer *r, Enesim_Color stroke_color);
EAPI Enesim_Color enesim_renderer_shape_stroke_color_get(Enesim_Renderer *r);

EAPI void enesim_renderer_shape_stroke_renderer_set(Enesim_Renderer *r, Enesim_Renderer *o);
EAPI Enesim_Renderer * enesim_renderer_shape_stroke_renderer_get(Enesim_Renderer *r);

EAPI void enesim_renderer_shape_stroke_weight_set(Enesim_Renderer *r, double weight);
EAPI double enesim_renderer_shape_stroke_weight_get(Enesim_Renderer *r);

EAPI void enesim_renderer_shape_stroke_location_set(Enesim_Renderer *r, Enesim_Renderer_Shape_Stroke_Location location);
EAPI Enesim_Renderer_Shape_Stroke_Location enesim_renderer_shape_stroke_location_get(Enesim_Renderer *r);

EAPI void enesim_renderer_shape_stroke_cap_set(Enesim_Renderer *r, Enesim_Renderer_Shape_Stroke_Cap cap);
EAPI Enesim_Renderer_Shape_Stroke_Cap enesim_renderer_shape_stroke_cap_get(Enesim_Renderer *r);

EAPI void enesim_renderer_shape_stroke_join_set(Enesim_Renderer *r, Enesim_Renderer_Shape_Stroke_Join join);
EAPI Enesim_Renderer_Shape_Stroke_Join enesim_renderer_shape_stroke_join_get(Enesim_Renderer *r);

EAPI void enesim_renderer_shape_stroke_renderer_set(Enesim_Renderer *r, Enesim_Renderer *o);
EAPI Enesim_Renderer * enesim_renderer_shape_stroke_renderer_get(Enesim_Renderer *r);

EAPI void enesim_renderer_shape_stroke_dash_add_simple(Enesim_Renderer *r, double length, double gap);
EAPI void enesim_renderer_shape_stroke_dash_add(Enesim_Renderer *r, const Enesim_Renderer_Shape_Stroke_Dash *dash);
EAPI void enesim_renderer_shape_stroke_dash_clear(Enesim_Renderer *r);
/* fill properties */
EAPI void enesim_renderer_shape_fill_color_set(Enesim_Renderer *r, Enesim_Color fill_color);
EAPI Enesim_Color enesim_renderer_shape_fill_color_get(Enesim_Renderer *r);

EAPI void enesim_renderer_shape_fill_renderer_set(Enesim_Renderer *r, Enesim_Renderer *f);
EAPI Enesim_Renderer * enesim_renderer_shape_fill_renderer_get(Enesim_Renderer *r);

EAPI void enesim_renderer_shape_fill_rule_set(Enesim_Renderer *r, Enesim_Renderer_Shape_Fill_Rule rule);
EAPI Enesim_Renderer_Shape_Fill_Rule enesim_renderer_shape_fill_rule_get(Enesim_Renderer *r);

/* stroke and/or fill */
EAPI void enesim_renderer_shape_draw_mode_set(Enesim_Renderer *r, Enesim_Renderer_Shape_Draw_Mode draw_mode);
EAPI Enesim_Renderer_Shape_Draw_Mode enesim_renderer_shape_draw_mode_get(Enesim_Renderer *r);

EAPI Eina_Bool enesim_renderer_shape_geometry_get(Enesim_Renderer *r, Enesim_Rectangle *geometry);
EAPI Eina_Bool enesim_renderer_shape_destination_geometry_get(
		Enesim_Renderer *r, Enesim_Rectangle *geometry);

/**
 * @}
 */

#endif
