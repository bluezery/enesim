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
#ifndef ENESIM_OPENGL_H_
#define ENESIM_OPENGL_H_

#define ENESIM_OPENGL_SHADER(k) #k

#include "Enesim.h"
#include "GL/gl.h"

EAPI Enesim_Pool * enesim_pool_opengl_new(void);

#ifdef ENESIM_EXTENSION
typedef struct _Enesim_Renderer_OpenGL_Compiled_Program
{
	GLenum id;
	GLenum *shaders;
	int num_shaders;
} Enesim_Renderer_OpenGL_Compiled_Program;

typedef struct _Enesim_Renderer_OpenGL_Data
{
	/* data fetch on the setup */
	Enesim_Renderer_OpenGL_Define_Geometry define_geometry;
	Enesim_Renderer_OpenGL_Shader_Setup shader_setup;
	Enesim_Renderer_OpenGL_Program *programs;
	int num_programs;
	/* generated */
	Enesim_Renderer_OpenGL_Compiled_Program *c_programs;
	GLuint fbo;
	/* FIXME remove this */
	GLenum program;
	Eina_Bool has_geometry; /* has a geometry shader */
	Eina_Bool has_vertex; /* has a vertex shader */
	Eina_Bool does_geometry; /* the renderer defines the geometry */
} Enesim_Renderer_OpenGL_Data;

typedef struct _Enesim_Buffer_OpenGL_Data
{
	GLuint texture;
	unsigned int num_textures;
} Enesim_Buffer_OpenGL_Data;
#endif

#endif

