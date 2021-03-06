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
#ifndef ENESIM_RENDERER_DISPMAP_H_
#define ENESIM_RENDERER_DISPMAP_H_

/**
 * @file
 * @ender_group{Enesim_Renderer_Dispmap}
 */

/**
 * @defgroup Enesim_Renderer_Dispmap Displacement Map
 * @brief Renderer that displays source pixels using a map image @ender_inherits{Enesim_Renderer}
 * @ingroup Enesim_Renderer
 * @{
 */

EAPI Enesim_Renderer * enesim_renderer_dispmap_new(void);
EAPI void enesim_renderer_dispmap_map_surface_set(Enesim_Renderer *r, Enesim_Surface *map);
EAPI Enesim_Surface * enesim_renderer_dispmap_map_surface_get(Enesim_Renderer *r);

EAPI void enesim_renderer_dispmap_source_surface_set(Enesim_Renderer *r, Enesim_Surface *src);
EAPI Enesim_Surface * enesim_renderer_dispmap_source_surface_get(Enesim_Renderer *r);

EAPI void enesim_renderer_dispmap_factor_set(Enesim_Renderer *r, double factor);
EAPI double enesim_renderer_dispmap_factor_get(Enesim_Renderer *r);

EAPI void enesim_renderer_dispmap_x_channel_set(Enesim_Renderer *r, Enesim_Channel channel);
EAPI void enesim_renderer_dispmap_y_channel_set(Enesim_Renderer *r, Enesim_Channel channel);

/**
 * @}
 */

#endif
