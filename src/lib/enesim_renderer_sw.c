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
/**
 * @todo
 * - In a near future we should move the API functions into global ones?
 */
/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/
#ifdef BUILD_PTHREAD
typedef struct _Enesim_Renderer_Thread_Operation
{
	/* common attributes */
	Enesim_Renderer *renderer;
	Enesim_Renderer_Sw_Fill fill;
	uint8_t * dst;
	size_t stride;
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
		uint8_t *ddata, size_t stride,
		uint8_t *tmp, size_t len, Eina_Rectangle *area)
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
		Enesim_Renderer_Sw_Fill fill, uint8_t *ddata,
		size_t stride, Eina_Rectangle *area)
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
		uint8_t *ddata, size_t stride,
		uint8_t *tmp, size_t len, Eina_Rectangle *area)
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
		Enesim_Renderer_Sw_Fill fill, uint8_t *ddata,
		size_t stride, Eina_Rectangle *area)
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
			uint8_t *tmp;
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
	Enesim_Renderer_Sw_Data *sw_data;

	sw_data = r->backend_data[ENESIM_BACKEND_SOFTWARE];
	/* fill the data needed for every threaded renderer */
	_op.renderer = r;
	_op.dst = ddata;
	_op.stride = stride;
	_op.area = *area;
	_op.fill = sw_data->fill;
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
	Enesim_Renderer_Sw_Data *sw_data;

	sw_data = r->backend_data[ENESIM_BACKEND_SOFTWARE];
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
		_sw_surface_draw_composed(r, sw_data->fill, span, ddata, stride, fdata, len, area);
	}
	else
	{
		_sw_surface_draw_simple(r, sw_data->fill, ddata, stride, area);
	}
end:
	return;
}
#endif


/*============================================================================*
 *                                 Global                                     *
 *============================================================================*/
void enesim_renderer_sw_init(void)
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

void enesim_renderer_sw_shutdown(void)
{
#ifdef BUILD_PTHREAD
	/* destroy the threads */
	free(_threads);
	pthread_barrier_destroy(&_start);
	pthread_barrier_destroy(&_end);
#endif
}


void enesim_renderer_sw_draw(Enesim_Renderer *r, Enesim_Surface *s, Eina_Rectangle *area,
		int x, int y, Enesim_Renderer_Flag flags)
{
	void *buffer_data;
	Enesim_Buffer_Sw_Data *data;
	Enesim_Format dfmt;
	uint8_t *ddata;
	size_t stride;

	buffer_data = enesim_surface_backend_data_get(s);
	data = s->buffer->backend_data;
	/* FIXME */
	stride = data->argb8888_pre.plane0_stride;
	ddata = data->argb8888_pre.plane0;

	dfmt = enesim_surface_format_get(s);
	ddata = ddata + (area->y * stride) + area->x;

	/* translate the origin */
	area->x -= x;
	area->y -= y;

	printf("stride = %d\n", stride);
#ifdef BUILD_PTHREAD
	_sw_draw_threaded(r, area, ddata, stride, dfmt, flags);
#else
	_sw_draw_no_threaded(r, area, ddata, stride, dfmt, flags);
#endif
}

void enesim_renderer_sw_draw_list(Enesim_Renderer *r, Enesim_Surface *s, Eina_Rectangle *area,
		Eina_List *clips, int x, int y, Enesim_Renderer_Flag flags)
{
	Enesim_Renderer_Sw_Data *rswdata;
	Enesim_Buffer_Sw_Data *data;
	Enesim_Format dfmt;
	uint8_t *ddata;
	size_t stride;

	dfmt = enesim_surface_format_get(s);
	data = s->buffer->backend_data;
	/* FIXME */
	stride = data->argb8888_pre.plane0_stride;
	ddata = data->argb8888_pre.plane0;
	ddata = ddata + (area->y * stride) + area->x;

	rswdata = r->backend_data[ENESIM_BACKEND_SOFTWARE];

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
			uint8_t *fdata;
			uint8_t *rdata;

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
			_sw_surface_draw_composed(r, rswdata->fill, span, rdata, stride,
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
			uint8_t *rdata;

			final = *clip;
			if (!eina_rectangle_intersection(&final, area))
				continue;
			rdata = ddata + (final.y * stride) + final.x;
			/* translate the origin */
			final.x -= x;
			final.y -= y;
			/* now render */
			_sw_surface_draw_simple(r, rswdata->fill, rdata, stride, &final);
		}
	}
end:
	return;
}
/*============================================================================*
 *                                   API                                      *
 *============================================================================*/
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI Eina_Bool enesim_renderer_sw_setup(Enesim_Renderer *r, Enesim_Surface *s,
		Enesim_Error **error)
{
	Enesim_Renderer_Sw_Fill fill;

	//ENESIM_MAGIC_CHECK_RENDERER(r);
	if (!r->descriptor->sw_setup) return EINA_TRUE;
	if (r->descriptor->sw_setup(r, s, &fill, error))
	{
		Enesim_Renderer_Sw_Data *sw_data;

		sw_data = enesim_renderer_backend_data_get(r, ENESIM_BACKEND_SOFTWARE);
		if (!sw_data)
		{
			sw_data = calloc(1, sizeof(Enesim_Renderer_Sw_Data));
			enesim_renderer_backend_data_set(r, ENESIM_BACKEND_SOFTWARE, sw_data);
		}
		sw_data->fill = fill;
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
	//ENESIM_MAGIC_CHECK_RENDERER(r);
	if (r->descriptor->sw_cleanup) r->descriptor->sw_cleanup(r);
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI Enesim_Renderer_Sw_Fill enesim_renderer_sw_fill_get(Enesim_Renderer *r)
{
	Enesim_Renderer_Sw_Data *sw_data;

	sw_data = r->backend_data[ENESIM_BACKEND_SOFTWARE];
	//ENESIM_MAGIC_CHECK_RENDERER(r);
	return sw_data->fill;
}
