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
#ifndef ENESIM_RENDERER_PRIVATE_H_
#define ENESIM_RENDERER_PRIVATE_H_

/*----------------------------------------------------------------------------*
 *                          State related functions                           *
 *----------------------------------------------------------------------------*/

typedef struct _Enesim_Renderer_State2
{
	struct {
		Enesim_Matrix transformation;
		Enesim_Matrix_Type transformation_type;
		Eina_Bool visibility;
		Enesim_Rop rop;
		Enesim_Color color;
		Enesim_Renderer *mask;
		double ox;
		double oy;
	} current, past;
	char *name;
	Eina_Bool changed;
} Enesim_Renderer_State2;

Eina_Bool enesim_renderer_state_transformation_set(Enesim_Renderer_State2 *thiz,
		const Enesim_Matrix *m);
Eina_Bool enesim_renderer_state_transformation_get(Enesim_Renderer_State2 *thiz,
		Enesim_Matrix *m);
void enesim_renderer_state_rop_set(Enesim_Renderer_State2 *thiz,
		Enesim_Rop rop);
Eina_Bool enesim_renderer_state_rop_get(Enesim_Renderer_State2 *thiz,
		Enesim_Rop *rop);
void enesim_renderer_state_visibility_set(Enesim_Renderer_State2 *thiz,
		Eina_Bool visibility);
Eina_Bool enesim_renderer_state_visibility_get(Enesim_Renderer_State2 *thiz,
		Eina_Bool *visibility);
void enesim_renderer_state_color_set(Enesim_Renderer_State2 *thiz,
		Enesim_Color color);
Eina_Bool enesim_renderer_state_color_get(Enesim_Renderer_State2 *thiz,
		Enesim_Color *color);
void enesim_renderer_state_x_set(Enesim_Renderer_State2 *thiz, double x);
Eina_Bool enesim_renderer_state_x_get(Enesim_Renderer_State2 *thiz, double *x);
void enesim_renderer_state_y_set(Enesim_Renderer_State2 *thiz, double y);
Eina_Bool enesim_renderer_state_y_get(Enesim_Renderer_State2 *thiz, double *y);
void enesim_renderer_state_commit(Enesim_Renderer_State2 *thiz);
Eina_Bool enesim_renderer_state_changed(Enesim_Renderer_State2 *thiz);
void enesim_renderer_state_clear(Enesim_Renderer_State2 *thiz);

typedef enum _Enesim_Renderer_States
{
	ENESIM_STATE_CURRENT,
	ENESIM_STATE_PAST,
	ENESIM_RENDERER_STATES,
} Enesim_Renderer_States;

typedef struct _Enesim_Renderer_State
{
	Eina_Bool visibility;
	Enesim_Rop rop;
	Enesim_Color color;
	Enesim_Quality quality;
	double ox, oy; /* the origin */
	Enesim_Renderer *mask;
	Enesim_Matrix transformation;
	Enesim_Matrix_Type transformation_type;
} Enesim_Renderer_State;


/*----------------------------------------------------------------------------*
 *                        Descriptor related functions                        *
 *----------------------------------------------------------------------------*/
/* common descriptor functions */
typedef const char * (*Enesim_Renderer_Name)(Enesim_Renderer *r);
typedef void (*Enesim_Renderer_Delete)(Enesim_Renderer *r);
typedef Eina_Bool (*Enesim_Renderer_Inside)(Enesim_Renderer *r, double x, double y);
typedef void (*Enesim_Renderer_Bounds)(Enesim_Renderer *r,
		const Enesim_Renderer_State *state[ENESIM_RENDERER_STATES],
		Enesim_Rectangle *rect);
typedef void (*Enesim_Renderer_Destination_Bounds)(Enesim_Renderer *r,
		const Enesim_Renderer_State *state[ENESIM_RENDERER_STATES],
		Eina_Rectangle *dbounds);
typedef void (*Enesim_Renderer_Flags)(Enesim_Renderer *r,
		const Enesim_Renderer_State *state,
		Enesim_Renderer_Flag *flags);
typedef void (*Enesim_Renderer_Hints_Get)(Enesim_Renderer *r,
		const Enesim_Renderer_State *state,
		Enesim_Renderer_Hint *hints);
typedef Eina_Bool (*Enesim_Renderer_Has_Changed)(Enesim_Renderer *r,
		const Enesim_Renderer_State *state[ENESIM_RENDERER_STATES]);
typedef void (*Enesim_Renderer_Damage)(Enesim_Renderer *r,
		const Eina_Rectangle *old_bounds,
		const Enesim_Renderer_State *state[ENESIM_RENDERER_STATES],
		Enesim_Renderer_Damage_Cb cb, void *data);

#if 0
/* TODO properties callbacks */
typedef void (*Enesim_Renderer_X_Set_Cb)(void *p, double x);
typedef double (*Enesim_Renderer_X_Get_Cb)(void *p);
typedef void (*Enesim_Renderer_Y_Set_Cb)(void *p, double x);
typedef double (*Enesim_Renderer_Y_Get_Cb)(void *p);
typedef void (*Enesim_Renderer_Transformation_Set_Cb)(void *p, Enesim_Matrix *m);
typedef void (*Enesim_Renderer_Transformation_Get_Cb)(void *p, Enesim_Matrix *m);
typedef void (*Enesim_Renderer_Quality_Set_Cb)(void *p, Enesim_Quality q);
typedef Enesim_Quality (*Enesim_Renderer_Quality_Get_Cb)(void *p);
#endif

/* software backend descriptor functions */
typedef void (*Enesim_Renderer_Sw_Fill)(Enesim_Renderer *r,
		const Enesim_Renderer_State *state,
		int x, int y,
		unsigned int len, void *dst);

typedef Eina_Bool (*Enesim_Renderer_Sw_Setup)(Enesim_Renderer *r,
		const Enesim_Renderer_State *state[ENESIM_RENDERER_STATES],
		Enesim_Surface *s,
		Enesim_Renderer_Sw_Fill *fill,
		Enesim_Error **error);
typedef void (*Enesim_Renderer_Sw_Cleanup)(Enesim_Renderer *r, Enesim_Surface *s);
/* opencl backend descriptor functions */
typedef Eina_Bool (*Enesim_Renderer_OpenCL_Setup)(Enesim_Renderer *r,
		const Enesim_Renderer_State *state[ENESIM_RENDERER_STATES],
		Enesim_Surface *s,
		const char **program_name, const char **program_source,
		size_t *program_length,
		Enesim_Error **error);
typedef void (*Enesim_Renderer_OpenCL_Cleanup)(Enesim_Renderer *r, Enesim_Surface *s);
typedef Eina_Bool (*Enesim_Renderer_OpenCL_Kernel_Setup)(Enesim_Renderer *r, Enesim_Surface *s);

/* OpenGL descriptor functions */
typedef Eina_Bool (*Enesim_Renderer_OpenGL_Initialize)(Enesim_Renderer *r,
		int *num_programs,
		Enesim_Renderer_OpenGL_Program ***programs);
typedef Eina_Bool (*Enesim_Renderer_OpenGL_Setup)(Enesim_Renderer *r,
		const Enesim_Renderer_State *state[ENESIM_RENDERER_STATES],
		Enesim_Surface *s,
		Enesim_Renderer_OpenGL_Draw *draw,
		Enesim_Error **error);
typedef void (*Enesim_Renderer_OpenGL_Cleanup)(Enesim_Renderer *r, Enesim_Surface *s);

typedef struct _Enesim_Renderer_Descriptor {
	/* common */
	unsigned int version;
	Enesim_Renderer_Name name;
	Enesim_Renderer_Delete free;
	Enesim_Renderer_Bounds bounds;
	Enesim_Renderer_Destination_Bounds destination_bounds;
	Enesim_Renderer_Flags flags;
	Enesim_Renderer_Hints_Get hints_get;
	Enesim_Renderer_Inside is_inside;
	Enesim_Renderer_Damage damage;
	Enesim_Renderer_Has_Changed has_changed;
	/* software based functions */
	Enesim_Renderer_Sw_Setup sw_setup;
	Enesim_Renderer_Sw_Cleanup sw_cleanup;
	/* opencl based functions */
	Enesim_Renderer_OpenCL_Setup opencl_setup;
	Enesim_Renderer_OpenCL_Kernel_Setup opencl_kernel_setup;
	Enesim_Renderer_OpenCL_Cleanup opencl_cleanup;
	/* opengl based functions */
	Enesim_Renderer_OpenGL_Initialize opengl_initialize;
	Enesim_Renderer_OpenGL_Setup opengl_setup;
	Enesim_Renderer_OpenGL_Cleanup opengl_cleanup;
	/* we should expand from here */
} Enesim_Renderer_Descriptor;

/*----------------------------------------------------------------------------*
 *                         Renderer related functions                         *
 *----------------------------------------------------------------------------*/
struct _Enesim_Renderer
{
	EINA_MAGIC
	char *name;
	int ref;
	Enesim_Renderer_State current;
	Enesim_Renderer_State past;
	/* the private data */
	Eina_Lock lock;
	Eina_Hash *prv_data;
	Enesim_Renderer_Flag current_flags;
	Enesim_Rectangle current_bounds;
	Enesim_Rectangle past_bounds;
	Eina_Rectangle current_destination_bounds;
	Eina_Rectangle past_destination_bounds;
	/* the descriptor */
	Enesim_Renderer_Descriptor descriptor;
	void *data;
	/* backend data */
	/* given that we can use the same renderer to draw into a software
	 * surface or opencl surface, we need an array to keep *ALL* the
	 * possible data */
	void *backend_data[ENESIM_BACKENDS];
	Eina_Bool in_setup : 1;
};

void enesim_renderer_init(void);
void enesim_renderer_shutdown(void);
void enesim_renderer_sw_free(Enesim_Renderer *r);
Enesim_Renderer * enesim_renderer_new(Enesim_Renderer_Descriptor
		*descriptor, void *data);
void * enesim_renderer_data_get(Enesim_Renderer *r);
void * enesim_renderer_backend_data_get(Enesim_Renderer *r, Enesim_Backend b);
void enesim_renderer_backend_data_set(Enesim_Renderer *r, Enesim_Backend b, void *data);

/* software related functions */
typedef struct _Enesim_Renderer_Sw_Data
{
	/* TODO for later we might need a pointer to the function that calls
	 *  the fill only or both, to avoid the if
	 */
	Enesim_Renderer_Sw_Fill fill;
	Enesim_Compositor_Span span;
} Enesim_Renderer_Sw_Data;

void enesim_renderer_sw_draw(Enesim_Renderer *r, int x, int y, unsigned int len, uint32_t *data);
void enesim_renderer_sw_init(void);
void enesim_renderer_sw_shutdown(void);
void enesim_renderer_sw_draw_area(Enesim_Renderer *r,
		Enesim_Surface *s, Eina_Rectangle *area,
		int x, int y);

Eina_Bool enesim_renderer_sw_setup(Enesim_Renderer *r, const Enesim_Renderer_State *state[ENESIM_RENDERER_STATES], Enesim_Surface *s, Enesim_Error **error);
void enesim_renderer_sw_cleanup(Enesim_Renderer *r, Enesim_Surface *s);

#if BUILD_OPENCL
Eina_Bool enesim_renderer_opencl_setup(Enesim_Renderer *r,
		const Enesim_Renderer_State *state[ENESIM_RENDERER_STATES],
		Enesim_Surface *s,
		Enesim_Error **error);
void enesim_renderer_opencl_cleanup(Enesim_Renderer *r, Enesim_Surface *s);
void enesim_renderer_opencl_draw(Enesim_Renderer *r, Enesim_Surface *s, Eina_Rectangle *area,
		int x, int y);
void enesim_renderer_opencl_init(void);
void enesim_renderer_opencl_shutdown(void);
void enesim_renderer_opencl_free(Enesim_Renderer *r);
#endif

#if BUILD_OPENGL
Eina_Bool enesim_renderer_opengl_setup(Enesim_Renderer *r,
		const Enesim_Renderer_State *state[ENESIM_RENDERER_STATES],
		Enesim_Surface *s,
		Enesim_Error **error);
void enesim_renderer_opengl_cleanup(Enesim_Renderer *r, Enesim_Surface *s);
void enesim_renderer_opengl_draw(Enesim_Renderer *r, Enesim_Surface *s, const Eina_Rectangle *area,
		int x, int y);

void enesim_renderer_opengl_init(void);
void enesim_renderer_opengl_shutdown(void);
void enesim_renderer_opengl_free(Enesim_Renderer *r);
#endif

#endif
