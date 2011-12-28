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
/*
 * Shall we use only one framebuffer? One framebuffer per renderer?
 */
/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/
/* FIXME for debugging purposes */
#define GLERR {\
        GLenum err; \
        err = glGetError(); \
        printf("Error %d\n", err); \
        }

static void _opengl_extensions_setup(void)
{
	char *extensions;

	extensions  = (char *)glGetString(GL_EXTENSIONS);
/*if( findString("GL_ARB_shader_objects",extensionList) &&
      findString("GL_ARB_shading_language_100",extensionList) &&
      findString("GL_ARB_vertex_shader",extensionList) &&
      findString("GL_ARB_fragment_shader",extensionList) )
"GL_EXT_geometry_shader4"
*/

}

/*============================================================================*
 *                                 Global                                     *
 *============================================================================*/
Eina_Bool enesim_renderer_opengl_setup(Enesim_Renderer *r,
		const Enesim_Renderer_State *state,
		Enesim_Surface *s,
		Enesim_Error **error)
{
	Enesim_Renderer_OpenGL_Data *rdata;
	Enesim_Buffer_OpenGL_Data *sdata;
	Eina_Bool ret;
	GLenum status;
	GLenum shader;
	const char *source = NULL;
	const char *source_name = NULL;
	size_t source_size = 0;

	sdata = enesim_surface_backend_data_get(s);
	rdata = enesim_renderer_backend_data_get(r, ENESIM_BACKEND_OPENGL);
	if (!rdata)
	{
		rdata = calloc(1, sizeof(Enesim_Renderer_OpenGL_Data));
		enesim_renderer_backend_data_set(r, ENESIM_BACKEND_OPENGL, rdata);
	}
	if (!r->descriptor.opengl_setup) return EINA_FALSE;
	ret = r->descriptor.opengl_setup(r, state, s, &source_name, &source, &source_size, error);
	if (!ret) return EINA_FALSE;
	/* setup the shaders */
	shader = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
	glShaderSourceARB(shader, 1, &source, &source_size);
	/* compile it */
	glCompileShaderARB(shader);
	if (!rdata->program)
	{
		rdata->program = glCreateProgramObjectARB();
	}
	/* attach the shader to the program */
	glAttachObjectARB(rdata->program, shader);
	/* link it */ 
	glLinkProgramARB(rdata->program);
 
	if (!rdata->fbo)
	{
		glGenFramebuffersEXT(1, &rdata->fbo);
	}
	/* attach the texture to the first color attachment */
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, rdata->fbo);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
			GL_TEXTURE_2D, sdata->texture, 0);

        status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
        if (status != GL_FRAMEBUFFER_COMPLETE_EXT)
        {
                printf("fb error %d\n", status);
        }

#if 0
	// Use The Program Object Instead Of Fixed Function OpenGL
	glUseProgramObjectARB(my_program);

	/* TODO get the shader from the renderer implementation */
	glUseProgramObjectARB(my_program);
	int my_vec3_location = glGetUniformLocationARB(my_program, "my_3d_vector");
	glUniform3fARB(my_vec3_location, 1.0f, 4.0f, 3.0f);
#endif

	return EINA_TRUE;
}

void enesim_renderer_opengl_cleanup(Enesim_Renderer *r, Enesim_Surface *s)
{
	Enesim_Renderer_OpenGL_Data *rdata;

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
}

void enesim_renderer_opengl_draw(Enesim_Renderer *r, Enesim_Surface *s, Eina_Rectangle *area,
		int x, int y, Enesim_Renderer_Flag flags)
{
	Enesim_Renderer_OpenGL_Data *rdata;
	Enesim_Buffer_OpenGL_Data *sdata;

	sdata = enesim_surface_backend_data_get(s);
	rdata = enesim_renderer_backend_data_get(r, ENESIM_BACKEND_OPENGL);

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, rdata->fbo);
	/* create the geometry to render to */
	/* FIXME for now */
	glEnable(GL_BLEND);
	glClearColor(0.0, 1.0, 0.0, 1.0);
	glColor3f(1.0, 0.0, 0.0);
	glBegin(GL_QUADS);
                glVertex2d(area->x, area->y);
                glVertex2d(area->x + area->w, area->y);
                glVertex2d(area->x + area->w, area->y + area->h);
                glVertex2d(area->x, area->y + area->h);
        glEnd();

	/* TODO get the compiled shader from the renderer backend data */
	/* TODO run it on the renderer fbo */
}

void enesim_renderer_opengl_init(void)
{
	/* maybe we should add a hash to match between kernel name and program */
}

void enesim_renderer_opengl_shutdown(void)
{

}

/* we need a way to destroy the renderer data
void enesim_renderer_opengl_delete(Enesim_Renderer *r)
{
	Enesim_Renderer_OpenGL_Data *rdata;
	rdata = enesim_renderer_backend_data_get(r, ENESIM_BACKEND_OPENGL);
	glDeleteFramebuffersEXT(1, &rdata->fbo);
}
*/
/*============================================================================*
 *                                   API                                      *
 *============================================================================*/

