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
#ifndef ENESIM_RENDERER_COMPOUND_H_
#define ENESIM_RENDERER_COMPOUND_H_

/**
 * @defgroup Enesim_Renderer_Compound_Group Compound
 * @ingroup Enesim_Renderer_Group
 * @{
 */

typedef Eina_Bool (*Enesim_Renderer_Compound_Cb)(Enesim_Renderer *r, Enesim_Renderer *layer, void *data);

EAPI Enesim_Renderer * enesim_renderer_compound_new(void);
EAPI void enesim_renderer_compound_layer_add(Enesim_Renderer *r,
		Enesim_Renderer *rend);
EAPI void enesim_renderer_compound_layer_remove(Enesim_Renderer *r,
		Enesim_Renderer *rend);
EAPI void enesim_renderer_compound_layer_clear(Enesim_Renderer *r);
EAPI void enesim_renderer_compound_layer_set(Enesim_Renderer *r,
		Eina_List *list);

EAPI void enesim_renderer_compound_pre_setup_set(Enesim_Renderer *r,
		Enesim_Renderer_Compound_Cb cb, void *data);
EAPI void enesim_renderer_compound_post_setup_set(Enesim_Renderer *r,
		Enesim_Renderer_Compound_Cb cb, void *data);

EAPI void enesim_renderer_compound_layer_foreach(Enesim_Renderer *r,
		Enesim_Renderer_Compound_Cb cb, void *data);
EAPI void enesim_renderer_compound_layer_reverse_foreach(Enesim_Renderer *r,
		Enesim_Renderer_Compound_Cb cb, void *data);

EAPI void enesim_renderer_compound_background_renderer_set(Enesim_Renderer *r, Enesim_Renderer *background);
/**
 * @}
 */

#endif

