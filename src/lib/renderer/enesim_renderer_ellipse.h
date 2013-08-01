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
#ifndef ENESIM_RENDERER_ELLIPSE_H_
#define ENESIM_RENDERER_ELLIPSE_H_

/**
 * @defgroup Enesim_Renderer_Ellipse_Group Ellipse
 * @brief Ellipse renderer
 * @ingroup Enesim_Renderer_Shape_Group
 * @{
 */
EAPI Enesim_Renderer * enesim_renderer_ellipse_new(void);

EAPI void enesim_renderer_ellipse_x_set(Enesim_Renderer *p, double x);
EAPI void enesim_renderer_ellipse_y_set(Enesim_Renderer *p, double y);
EAPI void enesim_renderer_ellipse_x_radius_set(Enesim_Renderer *p, double r);
EAPI void enesim_renderer_ellipse_y_radius_set(Enesim_Renderer *p, double r);
EAPI void enesim_renderer_ellipse_center_set(Enesim_Renderer *p, double x, double y);
EAPI void enesim_renderer_ellipse_center_get(Enesim_Renderer *p, double *x, double *y);
EAPI void enesim_renderer_ellipse_radii_set(Enesim_Renderer *p, double radius_x, double radius_y);
EAPI void enesim_renderer_ellipse_radii_get(Enesim_Renderer *p, double *radius_x, double *radius_y);
/**
 * @}
 */

#endif

