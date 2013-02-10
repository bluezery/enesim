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
#ifndef ENESIM_RENDERER_TEXT_BASE_PRIVATE_H_
#define ENESIM_RENDERER_TEXT_BASE_PRIVATE_H_

typedef struct _Enesim_Renderer_Text_Base_State
{
	unsigned int size;
	char *font_name;
	char *str;
} Enesim_Renderer_Text_Base_State;

typedef void (*Enesim_Renderer_Text_Base_Bounds)(Enesim_Renderer *r,
		const Enesim_Renderer_State *states[ENESIM_RENDERER_STATES],
		const Enesim_Renderer_Shape_State *sstates[ENESIM_RENDERER_STATES],
		const Enesim_Renderer_Text_Base_State *tstates[ENESIM_RENDERER_STATES],
		Enesim_Rectangle *bounds);
typedef void (*Enesim_Renderer_Text_Base_Destination_Bounds)(Enesim_Renderer *r,
		const Enesim_Renderer_State *states[ENESIM_RENDERER_STATES],
		const Enesim_Renderer_Shape_State *sstates[ENESIM_RENDERER_STATES],
		const Enesim_Renderer_Text_Base_State *tstates[ENESIM_RENDERER_STATES],
		Eina_Rectangle *bounds);

typedef Eina_Bool (*Enesim_Renderer_Text_Base_Sw_Setup)(Enesim_Renderer *r,
		const Enesim_Renderer_State *states[ENESIM_RENDERER_STATES],
		const Enesim_Renderer_Shape_State *sstates[ENESIM_RENDERER_STATES],
		const Enesim_Renderer_Text_Base_State *tstates[ENESIM_RENDERER_STATES],
		Enesim_Surface *s,
		Enesim_Renderer_Shape_Sw_Draw *fill,
		Enesim_Error **error);
typedef Eina_Bool (*Enesim_Renderer_Text_Base_OpenCL_Setup)(Enesim_Renderer *r,
		const Enesim_Renderer_State *states[ENESIM_RENDERER_STATES],
		const Enesim_Renderer_Shape_State *sstates[ENESIM_RENDERER_STATES],
		const Enesim_Renderer_Text_Base_State *tstates[ENESIM_RENDERER_STATES],
		Enesim_Surface *s,
		const char **program_name, const char **program_source,
		size_t *program_length,
		Enesim_Error **error);
typedef Eina_Bool (*Enesim_Renderer_Text_Base_OpenGL_Setup)(Enesim_Renderer *r,
		const Enesim_Renderer_State *states[ENESIM_RENDERER_STATES],
		const Enesim_Renderer_Shape_State *sstates[ENESIM_RENDERER_STATES],
		const Enesim_Renderer_Text_Base_State *tstates[ENESIM_RENDERER_STATES],
		Enesim_Surface *s,
		Enesim_Renderer_OpenGL_Draw *draw,
		Enesim_Error **error);

typedef struct _Enesim_Renderer_Text_Base_Descriptor
{
	/* common */
	Enesim_Renderer_Base_Name_Get_Cb name;
	Enesim_Renderer_Delete free;
	Enesim_Renderer_Text_Base_Bounds bounds;
	Enesim_Renderer_Text_Base_Destination_Bounds destination_bounds;
	Enesim_Renderer_Flags_Get_Cb flags;
	Enesim_Renderer_Hints_Get_Cb hints_get;
	Enesim_Renderer_Is_Inside_Cb is_inside;
	Enesim_Renderer_Damages_Get_Cb damage;
	Enesim_Renderer_Has_Changed_Cb has_changed;
	/* software based functions */
	Enesim_Renderer_Text_Base_Sw_Setup sw_setup;
	Enesim_Renderer_Sw_Cleanup sw_cleanup;
	/* opencl based functions */
	Enesim_Renderer_Text_Base_OpenCL_Setup opencl_setup;
	Enesim_Renderer_OpenCL_Kernel_Setup opencl_kernel_setup;
	Enesim_Renderer_OpenCL_Cleanup opencl_cleanup;
	/* opengl based functions */
	Enesim_Renderer_OpenGL_Initialize opengl_initialize;
	Enesim_Renderer_Text_Base_OpenGL_Setup opengl_setup;
	Enesim_Renderer_OpenGL_Cleanup opengl_cleanup;
} Enesim_Renderer_Text_Base_Descriptor;

Enesim_Renderer * enesim_renderer_text_base_new(Enesim_Text_Engine *engine, Enesim_Renderer_Text_Base_Descriptor *descriptor, void *data);
void * enesim_renderer_text_base_data_get(Enesim_Renderer *r);
void enesim_renderer_text_base_setup(Enesim_Renderer *r);

#endif


