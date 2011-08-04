/* ENESIM - Direct Rendering Library
 * Copyright (C) 2007-2008 Jorge Luis Zapata
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
/**
 * @todo
 * - Add a way to retrieve the damaged area between two different state_setup
 * i.e state_setup, state_cleanup, property_changes .... , damage_get,
 * state_setup, state_cleanup ... etc
 * something like: void (*damages)(Enesim_Renderer *r, Eina_List **damages);
 * - Add a class name to the description
 * - Add a way to get/set such description
 * - Add a multi cpu render, send each span to a different CPU. One way is to
 *   use a queue, we initialize every thread on startup and me them sleep
 *   then wakeup when the function is called and start enqueueing the span
 *   renders
 * - Add a scale tranform property, every renderer should multiply its
 *   coordinates and lengths by this multiplier
 * - Change every internal struct on Enesim to have the correct prefix
 *   looks like we are having issues with mingw
 * - We have some overlfow on the coordinates whenever we want to trasnlate or
 *   transform boundings, we need to fix the maximum and minimum for a
 *   coordinate and length
 *
 */
/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/
#define ENESIM_MAGIC_RENDERER 0xe7e51420
#define ENESIM_MAGIC_CHECK_RENDERER(d)\
	do {\
		if (!EINA_MAGIC_CHECK(d, ENESIM_MAGIC_RENDERER))\
			EINA_MAGIC_FAIL(d, ENESIM_MAGIC_RENDERER);\
	} while(0)

#ifdef BUILD_PTHREAD

typedef struct _Enesim_Renderer_Thread_Operation
{
	/* common attributes */
	Enesim_Renderer *renderer;
	Enesim_Renderer_Sw_Fill fill;
	uint32_t * dst;
	unsigned int stride;
	Eina_Rectangle area;
	/* in case the renderer needs to use a composer */
	Enesim_Compositor_Span span;
} Enesim_Renderer_Thread_Operation;

typedef struct _Enesim_Renderer_Thread
{
	int cpuidx;
	pthread_t tid;
} Enesim_Renderer_Thread;

static unsigned int _num_cpus;
static Enesim_Renderer_Thread *_threads;
static Enesim_Renderer_Thread_Operation _op;
static pthread_barrier_t _start;
static pthread_barrier_t _end;
#endif

static inline Eina_Bool _is_sw_draw_composed(Enesim_Renderer *r,
		Enesim_Renderer_Flag flags)
{
	if (((r->rop == ENESIM_FILL) && (r->color == ENESIM_COLOR_FULL))
			|| ((flags & ENESIM_RENDERER_FLAG_ROP) && (r->color == ENESIM_COLOR_FULL))
			|| ((flags & ENESIM_RENDERER_FLAG_ROP) && (flags & ENESIM_RENDERER_FLAG_COLORIZE)))
	{
		return EINA_FALSE;
	}
	return EINA_TRUE;
}

static inline void _sw_surface_draw_composed(Enesim_Renderer *r,
		Enesim_Renderer_Sw_Fill fill, Enesim_Compositor_Span span,
		uint32_t *ddata, uint32_t stride,
		uint32_t *tmp, size_t len, Eina_Rectangle *area)
{
	while (area->h--)
	{
		memset(tmp, 0, len);
		fill(r, area->x, area->y, area->w, tmp);
		area->y++;
		/* compose the filled and the destination spans */
		span(ddata, area->w, tmp, r->color, NULL);
		ddata += stride;
	}
}

static inline void _sw_surface_draw_simple(Enesim_Renderer *r,
		Enesim_Renderer_Sw_Fill fill, uint32_t *ddata,
		uint32_t stride, Eina_Rectangle *area)
{
	while (area->h--)
	{
		fill(r, area->x, area->y, area->w, ddata);
		area->y++;
		ddata += stride;
	}
}


#ifdef BUILD_PTHREAD

static inline void _sw_surface_draw_composed_threaded(Enesim_Renderer *r,
		unsigned int thread,
		Enesim_Renderer_Sw_Fill fill, Enesim_Compositor_Span span,
		uint32_t *ddata, uint32_t stride,
		uint32_t *tmp, size_t len, Eina_Rectangle *area)
{
	int h = area->h;
	int y = area->y;

	while (h)
	{
		if (h % _num_cpus != thread) goto end;

		memset(tmp, 0, len);
		fill(r, area->x, y, area->w, tmp);
		/* compose the filled and the destination spans */
		span(ddata, area->w, tmp, r->color, NULL);
end:
		ddata += stride;
		h--;
		y++;
	}
}

static inline void _sw_surface_draw_simple_threaded(Enesim_Renderer *r,
		unsigned int thread,
		Enesim_Renderer_Sw_Fill fill, uint32_t *ddata,
		uint32_t stride, Eina_Rectangle *area)
{
	int h = area->h;
	int y = area->y;

	while (h)
	{
		if (h % _num_cpus != thread) goto end;

		fill(r, area->x, y, area->w, ddata);
end:
		ddata += stride;
		h--;
		y++;
	}
}

static void * _thread_run(void *data)
{
	Enesim_Renderer_Thread *thiz = data;
	Enesim_Renderer_Thread_Operation *op = &_op;

	do {
		pthread_barrier_wait(&_start);
		if (op->span)
		{
			uint32_t *tmp;
			size_t len;

			len = op->area.w * sizeof(uint32_t);
			tmp = malloc(len);
			_sw_surface_draw_composed_threaded(op->renderer,
					thiz->cpuidx,
					op->fill,
					op->span,
					op->dst,
					op->stride,
					tmp,
					len,
					&op->area);
			free(tmp);
		}
		else
		{
			_sw_surface_draw_simple_threaded(op->renderer,
					thiz->cpuidx,
					op->fill,
					op->dst,
					op->stride,
					&op->area);

		}
		pthread_barrier_wait(&_end);
	} while (1);

	return NULL;
}

static void _sw_draw_threaded(Enesim_Renderer *r, Eina_Rectangle *area,
		uint32_t *ddata, size_t stride,
		Enesim_Format dfmt,
		Enesim_Renderer_Flag flags)
{
	/* fill the data needed for every threaded renderer */
	_op.renderer = r;
	_op.dst = ddata;
	_op.stride = stride;
	_op.area = *area;
	_op.fill = r->sw_fill;
	_op.span = NULL;

	if (_is_sw_draw_composed(r, flags))
	{
		Enesim_Compositor_Span span;

		span = enesim_compositor_span_get(r->rop, &dfmt, ENESIM_FORMAT_ARGB8888,
				r->color, ENESIM_FORMAT_NONE);
		if (!span)
		{
			WRN("No suitable span compositor to render %p with rop "
					"%d and color %08x", r, r->rop, r->color);
			goto end;
		}
		_op.span = span;
	}

	pthread_barrier_wait(&_start);
	pthread_barrier_wait(&_end);
end:
	return;
}
#else

static void _sw_draw_no_threaded(Enesim_Renderer *r, Eina_Rectangle *area,
		uint32_t *ddata, size_t stride, Enesim_Format dfmt,
		Enesim_Renderer_Flag flags)
{
	if (_is_sw_draw_composed(r, flags))
	{
		Enesim_Compositor_Span span;
		uint32_t *fdata;
		size_t len;

		span = enesim_compositor_span_get(r->rop, &dfmt, ENESIM_FORMAT_ARGB8888,
				r->color, ENESIM_FORMAT_NONE);
		if (!span)
		{
			WRN("No suitable span compositor to render %p with rop "
					"%d and color %08x", r, r->rop, r->color);
			goto end;
		}
		len = area->w * sizeof(uint32_t);
		fdata = alloca(len);
		_sw_surface_draw_composed(r, r->sw_fill, span, ddata, stride, fdata, len, area);
	}
	else
	{
		_sw_surface_draw_simple(r, r->sw_fill, ddata, stride, area);
	}
end:
	return;
}

#endif


static void _sw_draw(Enesim_Renderer *r, Enesim_Surface *s, Eina_Rectangle *area,
		int x, int y, Enesim_Renderer_Flag flags)
{
	Enesim_Buffer_Backend *buffer_data;
	Enesim_Buffer_Data *data;
	Enesim_Format dfmt;
	uint32_t *ddata;
	size_t stride;

	buffer_data = enesim_surface_backend_data_get(s);
	data = &buffer_data->data.sw_data;
	/* FIXME */
	stride = data->argb8888_pre.plane0_stride;
	ddata = data->argb8888_pre.plane0;

	dfmt = enesim_surface_format_get(s);
	ddata = ddata + (area->y * stride) + area->x;

	/* translate the origin */
	area->x -= x;
	area->y -= y;

#ifdef BUILD_PTHREAD
	_sw_draw_threaded(r, area, ddata, stride, dfmt, flags);
#else
	_sw_draw_no_threaded(r, area, ddata, stride, dfmt, flags);
#endif
}

static void _sw_draw_list(Enesim_Renderer *r, Enesim_Surface *s, Eina_Rectangle *area,
		Eina_List *clips, int x, int y, Enesim_Renderer_Flag flags)
{
	Enesim_Buffer_Backend *buffer_data;
	Enesim_Buffer_Data *data;
	Enesim_Format dfmt;
	uint32_t *ddata;
	size_t stride;

	dfmt = enesim_surface_format_get(s);
	data = &buffer_data->data.sw_data;
	/* FIXME */
	stride = data->argb8888_pre.plane0_stride;
	ddata = data->argb8888_pre.plane0;
	ddata = ddata + (area->y * stride) + area->x;

	if (_is_sw_draw_composed(r, flags))
	{
		Enesim_Compositor_Span span;
		Eina_Rectangle *clip;
		Eina_List *l;

		span = enesim_compositor_span_get(r->rop, &dfmt, ENESIM_FORMAT_ARGB8888,
				r->color, ENESIM_FORMAT_NONE);
		if (!span)
		{
			WRN("No suitable span compositor to render %p with rop "
					"%d and color %08x", r, r->rop, r->color);
			goto end;
		}
		/* iterate over the list of clips */
		EINA_LIST_FOREACH(clips, l, clip)
		{
			Eina_Rectangle final;
			size_t len;
			uint32_t *fdata;
			uint32_t *rdata;

			final = *clip;
			if (!eina_rectangle_intersection(&final, area))
				continue;
			rdata = ddata + (final.y * stride) + final.x;
			/* translate the origin */
			final.x -= x;
			final.y -= y;
			/* now render */
			len = final.w * sizeof(uint32_t);
			fdata = alloca(len);
			_sw_surface_draw_composed(r, r->sw_fill, span, rdata, stride,
					fdata, len, &final);
		}
	}
	else
	{
		Eina_Rectangle *clip;
		Eina_List *l;

		/* iterate over the list of clips */
		EINA_LIST_FOREACH(clips, l, clip)
		{
			Eina_Rectangle final;
			uint32_t *rdata;

			final = *clip;
			if (!eina_rectangle_intersection(&final, area))
				continue;
			rdata = ddata + (final.y * stride) + final.x;
			/* translate the origin */
			final.x -= x;
			final.y -= y;
			/* now render */
			_sw_surface_draw_simple(r, r->sw_fill, rdata, stride, &final);
		}
	}
end:
	return;
}

static void _draw_internal(Enesim_Renderer *r, Enesim_Surface *s,
		Eina_Rectangle *area, int x, int y, Enesim_Renderer_Flag flags)
{
	Enesim_Backend b;

	b = enesim_surface_backend_get(s);
	switch (b)
	{
		case ENESIM_BACKEND_SOFTWARE:
		_sw_draw(r, s, area, x, y, flags);
		break;

		default:
		break;
	}
}

static void _draw_list_internal(Enesim_Renderer *r, Enesim_Surface *s,
		Eina_Rectangle *area,
		Eina_List *clips, int x, int y, Enesim_Renderer_Flag flags)
{
	Enesim_Backend b;

	b = enesim_surface_backend_get(s);
	switch (b)
	{
		case ENESIM_BACKEND_SOFTWARE:
		_sw_draw_list(r, s, area, clips, x, y, flags);
		break;

		default:
		break;
	}
}

static inline void _surface_boundings(Enesim_Surface *s, Eina_Rectangle *boundings)
{
	boundings->x = 0;
	boundings->y = 0;
	enesim_surface_size_get(s, &boundings->w, &boundings->h);
}

/*============================================================================*
 *                                 Global                                     *
 *============================================================================*/
void enesim_renderer_init(void)
{
#ifdef BUILD_PTHREAD
	int i = 0;
	pthread_attr_t attr;

	_num_cpus = eina_cpu_count();
	_threads = malloc(sizeof(Enesim_Renderer_Thread) * _num_cpus);

	pthread_barrier_init(&_start, NULL, _num_cpus + 1);
	pthread_barrier_init(&_end, NULL, _num_cpus + 1);
	pthread_attr_init(&attr);
	for (i = 0; i < _num_cpus; i++)
	{
		cpu_set_t cpu;

		CPU_ZERO(&cpu);
		CPU_SET(i, &cpu);
		_threads[i].cpuidx = i;
		pthread_create(&_threads[i].tid, &attr, _thread_run, (void *)&_threads[i]);
		pthread_setaffinity_np(_threads[i].tid, sizeof(cpu_set_t), &cpu);

	}
#endif
}

void enesim_renderer_shutdown(void)
{
#ifdef BUILD_PTHREAD
	/* destroy the threads */
	free(_threads);
	pthread_barrier_destroy(&_start);
	pthread_barrier_destroy(&_end);
#endif
}

void enesim_renderer_relative_matrix_set(Enesim_Renderer *r, Enesim_Renderer *rel,
		Enesim_Matrix *old_matrix)
{
	Enesim_Matrix rel_matrix, r_matrix;

	/* the relativeness of the matrix should be done only if both
	 * both renderers have the matrix flag set
	 */
	/* TODO should we use the f16p16 matrix? */
	/* multiply the matrix by the current transformation */
	enesim_renderer_transformation_get(r, &r_matrix);
	enesim_renderer_transformation_get(rel, old_matrix);
	enesim_matrix_compose(old_matrix, &r_matrix, &rel_matrix);
	enesim_renderer_transformation_set(rel, &rel_matrix);
}

void enesim_renderer_relative_set(Enesim_Renderer *r, Enesim_Renderer *rel,
		Enesim_Matrix *old_matrix, double *old_ox, double *old_oy)
{
	Enesim_Matrix rel_matrix, r_matrix;
	double r_ox, r_oy;
	double nox, noy;

	if (!rel) return;

	/* TODO should we use the f16p16 matrix? */
	/* multiply the matrix by the current transformation */
	enesim_renderer_transformation_get(r, &r_matrix);
	enesim_renderer_transformation_get(rel, old_matrix);
	enesim_matrix_compose(old_matrix, &r_matrix, &rel_matrix);
	enesim_renderer_transformation_set(rel, &rel_matrix);
	/* add the origin by the current origin */
	enesim_renderer_origin_get(rel, old_ox, old_oy);
	enesim_renderer_origin_get(r, &r_ox, &r_oy);
	/* FIXME what to do with the origin?
	 * - if we use the first case we are also translating the rel origin to the
	 * parent origin * old_matrix
	 */
#if 1
	enesim_matrix_point_transform(old_matrix, *old_ox + r_ox, *old_oy + r_oy, &nox, &noy);
	enesim_renderer_origin_set(rel, nox, noy);
#else
	//printf("setting new origin %g %g\n", *old_ox - r_ox, *old_oy - r_oy);
	enesim_renderer_origin_set(rel, *old_ox - r_ox, *old_oy - r_oy);
#endif
}

void enesim_renderer_relative_unset(Enesim_Renderer *r, Enesim_Renderer *rel,
		Enesim_Matrix *old_matrix, double old_ox, double old_oy)
{
	if (!rel) return;

	/* restore origin */
	enesim_renderer_origin_set(rel, old_ox, old_oy);
	/* restore original matrix */
	enesim_renderer_transformation_set(rel, old_matrix);
}
/*============================================================================*
 *                                   API                                      *
 *============================================================================*/
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI Enesim_Renderer * enesim_renderer_new(Enesim_Renderer_Descriptor
		*descriptor, void *data)
{
	Enesim_Renderer *r;

	if (!descriptor) return NULL;
	if (descriptor->version > ENESIM_RENDERER_API) {
		ERR("API version %d is greater than %d", descriptor->version, ENESIM_RENDERER_API);
		return NULL;
	}

	r = calloc(1, sizeof(Enesim_Renderer));
	/* first check the passed in functions */
	if (!descriptor->is_inside) WRN("No is_inside() function available");
	if (!descriptor->boundings) WRN("No bounding() function available");
	if (!descriptor->flags) WRN("No flags() function available");
	if (!descriptor->sw_setup) WRN("No sw_setup() function available");
	if (!descriptor->sw_cleanup) WRN("No sw_setup() function available");
	if (!descriptor->free) WRN("No sw_setup() function available");
	r->descriptor = descriptor;
	r->data = data;
	/* now initialize the renderer common properties */
	EINA_MAGIC_SET(r, ENESIM_MAGIC_RENDERER);
	/* common properties */
	r->ox = 0;
	r->oy = 0;
	r->color = ENESIM_COLOR_FULL;
	r->rop = ENESIM_FILL;
	enesim_f16p16_matrix_identity(&r->matrix.values);
	enesim_matrix_identity(&r->matrix.original);
	r->matrix.type = ENESIM_MATRIX_IDENTITY;
	r->prv_data = eina_hash_string_superfast_new(NULL);

	return r;
}
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI Eina_Bool enesim_renderer_sw_setup(Enesim_Renderer *r)
{
	Enesim_Renderer_Sw_Fill fill;

	ENESIM_MAGIC_CHECK_RENDERER(r);
	if (!r->descriptor->sw_setup) return EINA_TRUE;
	if (r->descriptor->sw_setup(r, &fill))
	{
		r->sw_fill = fill;
		return EINA_TRUE;
	}
	WRN("Setup callback on %p failed", r);
	return EINA_FALSE;
}
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_sw_cleanup(Enesim_Renderer *r)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	if (r->descriptor->sw_cleanup) r->descriptor->sw_cleanup(r);
}
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI Enesim_Renderer_Sw_Fill enesim_renderer_sw_fill_get(Enesim_Renderer *r)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	return r->sw_fill;
}
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void * enesim_renderer_data_get(Enesim_Renderer *r)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);

	return r->data;
}

/**
 * Sets the transformation matrix of a renderer
 * @param[in] r The renderer to set the transformation matrix on
 * @param[in] m The transformation matrix to set
 */
EAPI void enesim_renderer_transformation_set(Enesim_Renderer *r, Enesim_Matrix *m)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);

	if (!m)
	{
		enesim_f16p16_matrix_identity(&r->matrix.values);
		enesim_matrix_identity(&r->matrix.original);
		r->matrix.type = ENESIM_MATRIX_IDENTITY;
		return;
	}
	r->matrix.original = *m;
	enesim_matrix_f16p16_matrix_to(m, &r->matrix.values);
	r->matrix.type = enesim_f16p16_matrix_type_get(&r->matrix.values);
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_transformation_get(Enesim_Renderer *r, Enesim_Matrix *m)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);

	if (m) *m = r->matrix.original;
}
/**
 * Deletes a renderer
 * @param[in] r The renderer to delete
 */
EAPI void enesim_renderer_delete(Enesim_Renderer *r)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	if (r->descriptor->free)
		r->descriptor->free(r);
	free(r);
}


/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI Eina_Bool enesim_renderer_setup(Enesim_Renderer *r, Enesim_Surface *s)
{
	Enesim_Backend b;

	b = enesim_surface_backend_get(s);
	switch (b)
	{
		case ENESIM_BACKEND_SOFTWARE:
		{
			Enesim_Renderer_Sw_Fill fill;

			if (!enesim_renderer_sw_setup(r)) return EINA_FALSE;
			fill = enesim_renderer_sw_fill_get(r);
			if (!fill)
			{
				enesim_renderer_sw_cleanup(r);
				return EINA_FALSE;
			}
			return EINA_TRUE;
		}
		break;
		default:
		break;
	}
	return EINA_FALSE;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_cleanup(Enesim_Renderer *r, Enesim_Surface *s)
{
	Enesim_Backend b;

	b = enesim_surface_backend_get(s);
	switch (b)
	{
		case ENESIM_BACKEND_SOFTWARE:
		enesim_renderer_sw_cleanup(r);
		break;

		default:
		break;
	}
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_flags(Enesim_Renderer *r, Enesim_Renderer_Flag *flags)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	if (r->descriptor->flags)
	{
		r->descriptor->flags(r, flags);
		return;
	}
	*flags = 0;
}
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_origin_set(Enesim_Renderer *r, double x, double y)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	r->ox = x;
	r->oy = y;
}
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_origin_get(Enesim_Renderer *r, double *x, double *y)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	if (x) *x = r->ox;
	if (y) *y = r->oy;
}

/**
 * To  be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_x_origin_set(Enesim_Renderer *r, double x)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	r->ox = x;
}

/**
 * To  be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_x_origin_get(Enesim_Renderer *r, double *x)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	if (x) *x = r->ox;
}

/**
 * To  be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_y_origin_set(Enesim_Renderer *r, double y)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	r->oy = y;
}

/**
 * To  be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_y_origin_get(Enesim_Renderer *r, double *y)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	if (y) *y = r->oy;
}
/**
 * To  be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_color_set(Enesim_Renderer *r, Enesim_Color color)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	r->color = color;
}
/**
 * To  be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_color_get(Enesim_Renderer *r, Enesim_Color *color)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	if (color) *color = r->color;
}

/**
 * Gets the bounding box of the renderer on its own coordinate space translated
 * by the origin
 * @param[in] r The renderer to get the boundings from
 * @param[out] rect The rectangle to store the boundings
 */
EAPI void enesim_renderer_translated_boundings(Enesim_Renderer *r, Enesim_Rectangle *boundings)
{
	Enesim_Renderer_Flag flags;
	ENESIM_MAGIC_CHECK_RENDERER(r);
	if (!boundings) return;

	enesim_renderer_boundings(r, boundings);
	enesim_renderer_flags(r, &flags);
	/* move by the origin */
#if LATER
	if (flags & ENESIM_RENDERER_FLAG_TRANSLATE)
#endif
	{
		if (boundings->x != INT_MIN / 2)
			boundings->x -= r->ox;
		if (boundings->y != INT_MIN / 2)
			boundings->y -= r->oy;
	}
}

/**
 * Gets the bounding box of the renderer on its own coordinate space without
 * adding the origin translation
 * @param[in] r The renderer to get the boundings from
 * @param[out] rect The rectangle to store the boundings
 */
EAPI void enesim_renderer_boundings(Enesim_Renderer *r, Enesim_Rectangle *rect)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	if (!rect) return;

	if (r->descriptor->boundings)
	{
		 r->descriptor->boundings(r, rect);
	}
	else
	{
		rect->x = INT_MIN / 2;
		rect->y = INT_MIN / 2;
		rect->w = INT_MAX;
		rect->h = INT_MAX;
	}
}

/**
 * To  be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_destination_boundings(Enesim_Renderer *r, Eina_Rectangle *rect,
		int x, int y)
{

	Enesim_Rectangle boundings;
	Enesim_Renderer_Flag flags;

	ENESIM_MAGIC_CHECK_RENDERER(r);

	if (!rect) return;

	enesim_renderer_boundings(r, &boundings);
	enesim_renderer_flags(r, &flags);
#if LATER
	if (flags & ENESIM_RENDERER_FLAG_TRANSLATE)
#endif
	{
		if (boundings.x != INT_MIN / 2)
			boundings.x += r->ox;
		if (boundings.y != INT_MIN / 2)
			boundings.y += r->oy;
	}
#if LATER
	if (flags & (ENESIM_RENDERER_FLAG_AFFINE | ENESIM_RENDERER_FLAG_PROJECTIVE))
#endif
	{
		if (r->matrix.type != ENESIM_MATRIX_IDENTITY && boundings.w != INT_MAX && boundings.h != INT_MAX)
		{
			Enesim_Quad q;
			Enesim_Matrix m;

			enesim_matrix_inverse(&r->matrix.original, &m);
			enesim_matrix_rectangle_transform(&m, &boundings, &q);
			enesim_quad_rectangle_to(&q, &boundings);
		}
	}
	rect->x = lround(boundings.x) - x;
	rect->y = lround(boundings.y) - y;
	rect->w = lround(boundings.w);
	rect->h = lround(boundings.h);
}

/**
 * To  be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_rop_set(Enesim_Renderer *r, Enesim_Rop rop)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	r->rop = rop;
}

/**
 * To  be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_rop_get(Enesim_Renderer *r, Enesim_Rop *rop)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	if (rop) *rop = r->rop;
}

/**
 * To  be documented
 * FIXME: To be fixed
 */
EAPI Eina_Bool enesim_renderer_is_inside(Enesim_Renderer *r, double x, double y)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	if (r->descriptor->is_inside) return r->descriptor->is_inside(r, x, y);
	return EINA_TRUE;
}

/**
 * To  be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_private_set(Enesim_Renderer *r, const char *name, void *data)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	eina_hash_add(r->prv_data, name, data);
}

/**
 * To  be documented
 * FIXME: To be fixed
 */
EAPI void * enesim_renderer_private_get(Enesim_Renderer *r, const char *name)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	return eina_hash_find(r->prv_data, name);
}

/**
 * Draw a renderer into a surface
 * @param[in] r The renderer to draw
 * @param[in] s The surface to draw the renderer into
 * @param[in] clip The area on the destination surface to limit the drawing
 * @param[in] x The x origin of the destination surface
 * @param[in] y The y origin of the destination surface
 * TODO What about the mask?
 */
EAPI void enesim_renderer_draw(Enesim_Renderer *r, Enesim_Surface *s,
		Eina_Rectangle *clip, int x, int y)
{
	Enesim_Renderer_Flag flags;
	Eina_Rectangle boundings;
	Eina_Rectangle final;

	ENESIM_MAGIC_CHECK_RENDERER(r);
	ENESIM_MAGIC_CHECK_SURFACE(s);

	if (!enesim_renderer_setup(r, s)) return;

	if (!clip)
	{
		_surface_boundings(s, &final);
	}
	else
	{
		Eina_Rectangle surface_size;

		final.x = clip->x;
		final.y = clip->y;
		final.w = clip->w;
		final.h = clip->h;
		_surface_boundings(s, &surface_size);
		if (!eina_rectangle_intersection(&final, &surface_size))
		{
			WRN("The renderer %p boundings does not intersect with the surface", r);
			goto end;
		}
	}
	/* clip against the destination rectangle */
	enesim_renderer_destination_boundings(r, &boundings, 0, 0);
	if (!eina_rectangle_intersection(&final, &boundings))
	{
		WRN("The renderer %p boundings does not intersect on the destination rectangle", r);
		goto end;
	}
	enesim_renderer_flags(r, &flags);
	_draw_internal(r, s, &final, x, y, flags);

	/* TODO set the format again */
end:
	enesim_renderer_cleanup(r, s);
}

/**
 * Draw a renderer into a surface
 * @param[in] r The renderer to draw
 * @param[in] s The surface to draw the renderer into
 * @param[in] clips A list of clipping areas on the destination surface to limit the drawing
 * @param[in] x The x origin of the destination surface
 * @param[in] y The y origin of the destination surface
 */
EAPI void enesim_renderer_draw_list(Enesim_Renderer *r, Enesim_Surface *s,
		Eina_List *clips, int x, int y)
{
	Enesim_Renderer_Flag flags;
	Eina_Rectangle boundings;
	Eina_Rectangle surface_size;

	if (!clips)
	{
		enesim_renderer_draw(r, s, NULL, x, y);
		return;
	}

	ENESIM_MAGIC_CHECK_RENDERER(r);
	ENESIM_MAGIC_CHECK_SURFACE(s);

	/* setup the common parameters */
	if (!enesim_renderer_setup(r, s)) return;

	_surface_boundings(s, &surface_size);
	/* clip against the destination rectangle */
	enesim_renderer_destination_boundings(r, &boundings, 0, 0);
	if (!eina_rectangle_intersection(&boundings, &surface_size))
	{
		WRN("The renderer %p boundings does not intersect on the destination rectangle", r);
		goto end;
	}
	enesim_renderer_flags(r, &flags);
	_draw_list_internal(r, s, &boundings, clips, x, y, flags);

	/* TODO set the format again */
end:
	enesim_renderer_cleanup(r, s);
}
