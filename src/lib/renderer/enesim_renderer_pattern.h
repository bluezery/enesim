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
#ifndef ENESIM_RENDERER_PATTERN_H_
#define ENESIM_RENDERER_PATTERN_H_

/**
 * @defgroup Enesim_Renderer_Pattern_Group Pattern
 * @brief Renderer that repeats an area of another renderer to form a pattern
 * @ingroup Enesim_Renderer_Group
 * @{
 */
EAPI Enesim_Renderer * enesim_renderer_pattern_new(void);
EAPI void enesim_renderer_pattern_width_set(Enesim_Renderer *r, double width);
EAPI double enesim_renderer_pattern_width_get(Enesim_Renderer *r);
EAPI void enesim_renderer_pattern_height_set(Enesim_Renderer *r, double height);
EAPI double enesim_renderer_pattern_height_get(Enesim_Renderer *r);
EAPI void enesim_renderer_pattern_x_set(Enesim_Renderer *r, double x);
EAPI double enesim_renderer_pattern_x_get(Enesim_Renderer *r);
EAPI void enesim_renderer_pattern_y_set(Enesim_Renderer *r, double y);
EAPI double enesim_renderer_pattern_y_get(Enesim_Renderer *r);
EAPI void enesim_renderer_pattern_position_set(Enesim_Renderer *r, double x, double y);
EAPI void enesim_renderer_pattern_position_get(Enesim_Renderer *r, double *x, double *y);
EAPI void enesim_renderer_pattern_size_set(Enesim_Renderer *r, double width, double height);
EAPI void enesim_renderer_pattern_size_get(Enesim_Renderer *r, double *width, double *height);
EAPI void enesim_renderer_pattern_source_renderer_set(Enesim_Renderer *r, Enesim_Renderer *source);
EAPI Enesim_Renderer * enesim_renderer_pattern_source_renderer_get(Enesim_Renderer *r);
EAPI void enesim_surface_pattern_source_surface_set(Enesim_Renderer *r, Enesim_Surface *source);
EAPI Enesim_Surface * enesim_surface_pattern_source_surface_get(Enesim_Renderer *r);
EAPI void enesim_renderer_pattern_repeat_mode_set(Enesim_Renderer *r, Enesim_Repeat_Mode mode);
EAPI Enesim_Repeat_Mode enesim_renderer_pattern_repeat_mode_get(Enesim_Renderer *r);

/**
 * @}
 */

#endif

