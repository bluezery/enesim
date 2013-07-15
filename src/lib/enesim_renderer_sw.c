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

/* eina includes sched.h for us, but does not set the _GNU_SOURCE
 * flag, so basically we cant use CPU_ZERO, CPU_SET, etc
 */

#include "enesim_private.h"

#include "enesim_main.h"
#include "enesim_log.h"
#include "enesim_color.h"
#include "enesim_rectangle.h"
#include "enesim_matrix.h"
#include "enesim_pool.h"
#include "enesim_buffer.h"
#include "enesim_surface.h"
#include "enesim_compositor.h"
#include "enesim_renderer.h"
#include "enesim_object_descriptor.h"
#include "enesim_object_class.h"
#include "enesim_object_instance.h"

#include "enesim_renderer_private.h"

/**
 * @todo
 * - In a near future we should move the API functions into global ones?
 */
/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/
#define ENESIM_LOG_DEFAULT enesim_log_renderer

static inline Eina_Bool _is_sw_draw_composed(Enesim_Color *color,
		Enesim_Rop *rop, Enesim_Renderer_Sw_Hint hints)
{
	if (hints & ENESIM_RENDERER_HINT_COLORIZE)
		*color = ENESIM_COLOR_FULL;
	if (hints & ENESIM_RENDERER_HINT_ROP)
		*rop = ENESIM_FILL;

	/* fill rop and color is full, we use the simple draw function */
	if ((*rop == ENESIM_FILL) && (*color == ENESIM_COLOR_FULL))
		return EINA_FALSE;
	return EINA_TRUE;
}

static inline void _sw_surface_setup(Enesim_Surface *s, Enesim_Format *dfmt, void **data, size_t *stride, size_t *bpp)
{
	Enesim_Buffer_Sw_Data *bdata;

	bdata = enesim_surface_backend_data_get(s);
	*dfmt = enesim_surface_format_get(s);
	if (*dfmt != ENESIM_FORMAT_A8)
	{
		*stride = bdata->argb8888_pre.plane0_stride;
		*data = bdata->argb8888_pre.plane0;
		*bpp = 4;
	}
	else
	{
		*stride = bdata->a8.plane0_stride;
		*data = bdata->a8.plane0;
		*bpp = 1;
	}
}

/* worst case, rop+color+mask(rop+color) */
/* rop+color+mask */
/* rop+mask */
/* color+mask */
/* mask */
/* color */
/* rop */

static inline void _sw_surface_draw_rop_mask(Enesim_Renderer *r,
		Enesim_Renderer_Sw_Fill fill,
		Enesim_Renderer_Sw_Fill mask_fill,
		Enesim_Compositor_Span span,
		Enesim_Compositor_Span mask_span EINA_UNUSED,
		uint8_t *ddata, size_t stride,
		uint8_t *tmp,
		uint8_t *tmp_mask,
		size_t len,
		Eina_Rectangle *area)
{
	Enesim_Renderer *mask;
	Enesim_Color color;

	enesim_renderer_mask_get(r, &mask);
	enesim_renderer_color_get(r, &color);

	while (area->h--)
	{
		/* FIXME we should not memset this */
		memset(tmp_mask, 0, len);
		memset(tmp, 0, len);

		fill(r, area->x, area->y, area->w, tmp);
		mask_fill(mask, area->x, area->y, area->w, tmp_mask);
		area->y++;
		/* compose the filled and the destination spans */
		span((uint32_t *)ddata, area->w, (uint32_t *)tmp, color, (uint32_t *)tmp_mask);
		ddata += stride;
	}
	if (mask)
		enesim_renderer_unref(mask);
}

/* rop = any (~FLAG_ROP)
 * color = any (~FLAG_COLORIZE)
 */
static inline void _sw_surface_draw_rop(Enesim_Renderer *r,
		Enesim_Renderer_Sw_Fill fill,
		Enesim_Compositor_Span span,
		uint8_t *ddata, size_t stride,
		uint8_t *tmp, size_t len,
		Eina_Rectangle *area)
{
	Enesim_Color color;

	enesim_renderer_color_get(r, &color);
	while (area->h--)
	{
		/* FIXME we should not memset this */
		memset(tmp, 0, len);
		fill(r, area->x, area->y, area->w, tmp);
		area->y++;
		/* compose the filled and the destination spans */
		span((uint32_t *)ddata, area->w, (uint32_t *)tmp, color, NULL);
		ddata += stride;
	}
}

/* rop = fill | any(FLAG_ROP)
 * color = none | any(FLAG_COLORIZE)
 * mask = none
 */
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

static inline void _sw_clear(uint8_t *ddata, size_t stride, int bpp,
		Eina_Rectangle *area)
{
	ddata = ddata + (area->y * stride) + (area->x * bpp);
	while (area->h--)
	{
		memset(ddata, 0x00, area->w * bpp);
		ddata += stride;
	}
}

#if 0
static inline void _sw_clear2(uint8_t *ddata, size_t stride, int bpp,
		Eina_Rectangle *area)
{
	ddata = ddata + (area->y * stride) + (area->x * bpp);
	printf("clearing2 %" EINA_EXTRA_RECTANGLE_FORMAT "\n", EINA_EXTRA_RECTANGLE_ARGS(area));
	while (area->h--)
	{
		memset(ddata, 0x33, area->w * bpp);
		ddata += stride;
	}
	printf("done\n");
}
#endif
/*----------------------------------------------------------------------------*
 *                            Threaded rendering                              *
 *----------------------------------------------------------------------------*/
#ifdef BUILD_THREAD

#ifdef _WIN32
typedef HANDLE enesim_thread;
#else
typedef pthread_t enesim_thread;
#endif
typedef struct _Enesim_Renderer_Thread_Operation
{
	/* common attributes */
	Enesim_Renderer *renderer;
	Enesim_Renderer_Sw_Fill fill;
	Enesim_Renderer_Sw_Fill mask_fill;
	uint8_t * dst;
	size_t stride;
	Eina_Rectangle area;
	/* in case the renderer needs to use a composer */
	Enesim_Compositor_Span span;
} Enesim_Renderer_Thread_Operation;

typedef struct _Enesim_Renderer_Thread
{
	int cpuidx;
	enesim_thread tid;
	Eina_Bool done;
} Enesim_Renderer_Thread;

static unsigned int _num_cpus;
static Enesim_Renderer_Thread *_threads;
static Enesim_Renderer_Thread_Operation _op;
#ifdef _WIN32
typedef struct _Enesim_Barrier Enesim_Barrier;
struct _Enesim_Barrier
{
   int needed, called;
   Eina_Lock cond_lock;
   Eina_Condition cond;
};
static Enesim_Barrier _start;
static Enesim_Barrier _end;
static Eina_Bool
enesim_barrier_new(Enesim_Barrier *barrier, int needed)
{
   barrier->needed = needed;
   barrier->called = 0;
   if (!eina_lock_new(&(barrier->cond_lock)))
     return EINA_FALSE;
   if (!eina_condition_new(&(barrier->cond), &(barrier->cond_lock)))
     return EINA_FALSE;
   return EINA_TRUE;
}

static void
enesim_barrier_free(Enesim_Barrier *barrier)
{
   eina_condition_free(&(barrier->cond));
   eina_lock_free(&(barrier->cond_lock));
   barrier->needed = 0;
   barrier->called = 0;
}

static Eina_Bool
enesim_barrier_wait(Enesim_Barrier *barrier)
{
   eina_lock_take(&(barrier->cond_lock));
   barrier->called++;
   if (barrier->called == barrier->needed)
     {
        barrier->called = 0;
        eina_condition_broadcast(&(barrier->cond));
     }
   else
     eina_condition_wait(&(barrier->cond));
   eina_lock_release(&(barrier->cond_lock));
   return EINA_TRUE;
}
static Eina_Bool
enesim_thread_new(HANDLE *thread, LPTHREAD_START_ROUTINE callback, void *data)
{
	*thread = CreateThread(NULL, 0, callback, data, 0, NULL);
	return *thread != NULL;
}
#define enesim_thread_free(thread) CloseHandle(thread)

#define _enesim_affinity_set(thread, cpunum) SetThreadAffinityMask(thread, 1 << (cpunum))
#else
static pthread_barrier_t _start;
static pthread_barrier_t _end;
#define _enesim_affinity_set(thread, cpunum) \
do \
{ \
	enesim_cpu_set_t cpu; \
	CPU_ZERO(&cpu); \
	CPU_SET(cpunum, &cpu); \
	pthread_setaffinity_np(thread, sizeof(enesim_cpu_set_t), &cpu); \
} while (0)
#define enesim_barrier_new(barrier, needed) pthread_barrier_init(barrier, NULL, needed)
#define enesim_barrier_free(barrier) pthread_barrier_destroy(barrier)
#define enesim_barrier_wait(barrier) pthread_barrier_wait(barrier)
static Eina_Bool
enesim_thread_new(pthread_t *thread, void *(*callback)(void *d), void *data)
{
	pthread_attr_t attr;

	pthread_attr_init(&attr);
	return pthread_create(thread, &attr, callback, data);
}
#define enesim_thread_free(thread) pthread_join(thread, NULL)
#endif

static inline void _sw_surface_draw_rop_threaded(Enesim_Renderer *r,
		unsigned int thread,
		Enesim_Renderer_Sw_Fill fill, Enesim_Compositor_Span span,
		uint8_t *ddata, size_t stride,
		uint8_t *tmp, size_t len, Eina_Rectangle *area)
{
	Enesim_Color color;
	int h = area->h;
	int y = area->y;

	enesim_renderer_color_get(r, &color);
	while (h)
	{
		if (h % _num_cpus != thread) goto end;

		/* FIXME we should not memset this */
		memset(tmp, 0, len);
		fill(r, area->x, y, area->w, tmp);
		/* compose the filled and the destination spans */
		span((uint32_t *)ddata, area->w, (uint32_t *)tmp, color, NULL);
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

#ifdef _WIN32
static DWORD WINAPI _thread_run(void *data)
#else
static void * _thread_run(void *data)
#endif
{
	Enesim_Renderer_Thread *thiz = data;
	Enesim_Renderer_Thread_Operation *op = &_op;

	do
	{
		enesim_barrier_wait(&_start);
		if (thiz->done) goto end;

		if (op->span)
		{
			uint8_t *tmp;
			size_t len;

			len = op->area.w * sizeof(uint32_t);
			/* FIXME remove this malloc. or we either
			 * make the tmp buffer part of the renderer
			 * and make it grow until we reach the span len
			 * or alloca everytime
			 */
			tmp = malloc(len);
			_sw_surface_draw_rop_threaded(op->renderer,
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
		enesim_barrier_wait(&_end);
	} while (1);

end:
#ifdef _WIN32
	return 0;
#else
	return NULL;
#endif
}

static void _sw_draw_threaded(Enesim_Renderer *r, Eina_Rectangle *area,
		uint8_t *ddata, size_t stride,
		Enesim_Format dfmt EINA_UNUSED)
{
	Enesim_Renderer_Sw_Data *sw_data;

	sw_data = enesim_renderer_backend_data_get(r, ENESIM_BACKEND_SOFTWARE);
	/* fill the data needed for every threaded renderer */
	_op.renderer = r;
	_op.dst = ddata;
	_op.stride = stride;
	_op.area = *area;
	_op.fill = sw_data->fill;
	_op.mask_fill = NULL;
	_op.span = sw_data->span;

	enesim_barrier_wait(&_start);
	enesim_barrier_wait(&_end);
}
#else
/*----------------------------------------------------------------------------*
 *                          No threaded rendering                             *
 *----------------------------------------------------------------------------*/
static void _sw_draw_no_threaded(Enesim_Renderer *r,
		Eina_Rectangle *area,
		uint8_t *ddata, size_t stride,
		Enesim_Format dfmt EINA_UNUSED)
{
	Enesim_Renderer_Sw_Data *sw_data;

	sw_data = enesim_renderer_backend_data_get(r, ENESIM_BACKEND_SOFTWARE);
	if (sw_data->span)
	{
		uint8_t *fdata;
		size_t len;

		len = area->w * sizeof(uint32_t);
		fdata = alloca(len);
		_sw_surface_draw_rop(r, sw_data->fill, sw_data->span, ddata, stride, fdata, len, area);
	}
	else
	{
		_sw_surface_draw_simple(r, sw_data->fill, ddata, stride, area);
	}
}
#endif
/*============================================================================*
 *                                 Global                                     *
 *============================================================================*/
void enesim_renderer_sw_init(void)
{
#ifdef BUILD_THREAD
	unsigned int i = 0;

	_num_cpus = eina_cpu_count();
	_threads = malloc(sizeof(Enesim_Renderer_Thread) * _num_cpus);

	enesim_barrier_new(&_start, _num_cpus + 1);
	enesim_barrier_new(&_end, _num_cpus + 1);
	for (i = 0; i < _num_cpus; i++)
	{
		_threads[i].cpuidx = i;
		_threads[i].done = EINA_FALSE;
		enesim_thread_new(&_threads[i].tid, _thread_run, (void *)&_threads[i]);
		_enesim_affinity_set(_threads[i].tid, i);

	}
#endif
}

void enesim_renderer_sw_shutdown(void)
{
#ifdef BUILD_THREAD
	unsigned int i;

	/* first mark all the threads to leave */
	for (i = 0; i < _num_cpus; i++)
		_threads[i].done = EINA_TRUE;

	/* now increment the barrier so the threads start again */
	enesim_barrier_wait(&_start);
	/* destroy the threads */
	for (i = 0; i < _num_cpus; i++)
		enesim_thread_free(_threads[i].tid);
	free(_threads);
	enesim_barrier_free(&_start);
	enesim_barrier_free(&_end);
#endif
}

void enesim_renderer_sw_draw_area(Enesim_Renderer *r, Enesim_Surface *s, Eina_Rectangle *area,
		int x, int y)
{
	Enesim_Format dfmt;
	Eina_Rectangle final;
	Eina_Bool visible;
	Eina_Bool intersect;
	uint8_t *ddata;
	size_t stride;
	size_t bpp;

	/* get the destination pointer */
	_sw_surface_setup(s, &dfmt, (void **)&ddata, &stride, &bpp);

	/* be sure to clip the area to the renderer bounds */
	final = r->current_destination_bounds;
	/* final translation */
	final.x -= x;
	final.x -= y;

	intersect = eina_rectangle_intersection(&final, area);
	if (r->state.current.rop == ENESIM_FILL)
	{
		/* just memset the whole area */
		if (!intersect)
		{
			_sw_clear(ddata, stride, bpp, area);
			return;
		}
		/* clear the difference rectangle */
		else
		{
			Eina_Rectangle subs[4];
			int i;

			eina_extra_rectangle_substract(area, &final, subs);
			for (i = 0; i < 4; i++)
			{
				if (!eina_extra_rectangle_is_valid(&subs[i]))
					continue;
				_sw_clear(ddata, stride, bpp, &subs[i]);
			}
		}
	}

	enesim_renderer_visibility_get(r, &visible);
	if (!visible)
		return;
	if (!intersect || !eina_extra_rectangle_is_valid(&final))
		return;

	ddata = ddata + (final.y * stride) + (final.x * bpp);
#ifdef BUILD_THREAD
	_sw_draw_threaded(r, &final, ddata, stride, dfmt);
#else
	_sw_draw_no_threaded(r, &final, ddata, stride, dfmt);
#endif
}

Eina_Bool enesim_renderer_sw_setup(Enesim_Renderer *r,
		Enesim_Surface *s,
		Enesim_Log **error)
{
	Enesim_Renderer_Class *klass;
	Enesim_Renderer_Sw_Fill fill = NULL;
	Enesim_Compositor_Span span = NULL;
	Enesim_Renderer_Sw_Data *sw_data;
	Enesim_Renderer_Sw_Hint hints;
	Enesim_Renderer *mask;
	Enesim_Color color;
	Enesim_Rop rop;
	const char *name;

	klass = ENESIM_RENDERER_CLASS_GET(r);
	enesim_renderer_name_get(r, &name);
	enesim_renderer_mask_get(r, &mask);
	enesim_renderer_color_get(r, &color);
	enesim_renderer_rop_get(r, &rop);

	/* do the setup on the mask */
	/* FIXME later this should be merged on the common renderer code */
	if (mask)
	{
		Eina_Bool ret;

		ret = enesim_renderer_setup(mask, s, error);
		enesim_renderer_unref(mask);
		if (!ret)
		{
			WRN("Mask setup callback on '%s' failed", name);
			return EINA_FALSE;
		}
	}
	if (!klass->sw_setup) return EINA_TRUE;
	if (!klass->sw_setup(r, s, &fill, error))
	{
		WRN("Setup callback on '%s' failed", name);
		return EINA_FALSE;

	}
	if (!fill)
	{
		ENESIM_RENDERER_LOG(r, error, "Even if the setup did not failed, there's no fill function");
		enesim_renderer_sw_cleanup(r, s);
		return EINA_FALSE;
	}

	sw_data = enesim_renderer_backend_data_get(r, ENESIM_BACKEND_SOFTWARE);
	if (!sw_data)
	{
		sw_data = calloc(1, sizeof(Enesim_Renderer_Sw_Data));
		enesim_renderer_backend_data_set(r, ENESIM_BACKEND_SOFTWARE, sw_data);
	}

	enesim_renderer_sw_hints_get(r, &hints);
	if (_is_sw_draw_composed(&color, &rop, hints))
	{
		Enesim_Format dfmt;

		dfmt = enesim_surface_format_get(s);
		span = enesim_compositor_span_get(rop, &dfmt, ENESIM_FORMAT_ARGB8888,
				color, ENESIM_FORMAT_NONE);
		if (!span)
		{
			WRN("No suitable span compositor to render %p with rop "
					"%d and color %08x", r, rop, color);
			return EINA_FALSE;
		}
	}

	/* TODO add a real_draw function that will compose the two ... or not :) */
	sw_data->span = span;
	sw_data->fill = fill;
	return EINA_TRUE;
}

void enesim_renderer_sw_cleanup(Enesim_Renderer *r, Enesim_Surface *s)
{
	Enesim_Renderer *mask;
	Enesim_Renderer_Class *klass;

	klass = ENESIM_RENDERER_CLASS_GET(r);
	enesim_renderer_mask_get(r, &mask);
	/* do the setup on the mask */
	/* FIXME later this should be merged on the common renderer code */
	if (mask)
	{
		enesim_renderer_cleanup(mask, s);
		enesim_renderer_unref(mask);
	}
	if (klass->sw_cleanup) klass->sw_cleanup(r, s);
}

void enesim_renderer_sw_free(Enesim_Renderer *r)
{
	Enesim_Renderer_Sw_Data *sw_data;

	sw_data = enesim_renderer_backend_data_get(r, ENESIM_BACKEND_SOFTWARE);
	if (!sw_data) return;
	free(sw_data);
}
/*============================================================================*
 *                                   API                                      *
 *============================================================================*/
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_sw_draw(Enesim_Renderer *r, int x, int y, unsigned int len, uint32_t *data)
{
	Enesim_Renderer_Sw_Data *sw_data;
	Eina_Rectangle span;
	Eina_Rectangle rbounds;
	Eina_Bool visible;
	unsigned int left;
	unsigned int right;

	enesim_renderer_visibility_get(r, &visible);
	if (!visible) return;
	sw_data = r->backend_data[ENESIM_BACKEND_SOFTWARE];

	eina_rectangle_coords_from(&span, x, y, len, 1);
	rbounds = r->current_destination_bounds;
	if (!eina_rectangle_intersection(&rbounds, &span))
	{
		DBG("Drawing span %" EINA_EXTRA_RECTANGLE_FORMAT " and bounds %"
				EINA_EXTRA_RECTANGLE_FORMAT " do not intersect on "
				"'%s'", EINA_EXTRA_RECTANGLE_ARGS (&span),
				EINA_EXTRA_RECTANGLE_ARGS (&rbounds),
				r->state.name);
		return;
	}
	left = rbounds.x - span.x;
	right = (span.x + span.w) - (rbounds.x + rbounds.w);
	if (sw_data->span)
	{
		Enesim_Color color;
		uint32_t *tmp;
		size_t bytes;

		enesim_renderer_color_get(r, &color);
		bytes = rbounds.w * sizeof(uint32_t);
		tmp = alloca(bytes);
		/* FIXME for now */
		memset(tmp, 0, bytes);
		sw_data->fill(r, rbounds.x, rbounds.y, rbounds.w, tmp);
		/* compose the filled and the destination spans */
		sw_data->span(data + left, rbounds.w, tmp, color, NULL);
	}
	else
	{
		sw_data->fill(r, rbounds.x, rbounds.y, rbounds.w, data + left);
	}
#if 0
	if (left > 0)
		memset(data, 0, left * sizeof(uint32_t));
	if (right > 0)
		memset(data + len - right, 0, right * sizeof(uint32_t));
#endif
#if 0
	if (sw_data->span)
	{
		uint32_t *tmp;
		size_t bytes;

		bytes = len * sizeof(uint32_t);
		tmp = alloca(bytes);
		/* FIXME for now */
		memset(tmp, 0, bytes);
		sw_data->fill(r, &r->current, x, y, len, tmp);
		/* compose the filled and the destination spans */
		sw_data->span(data, len, tmp, r->current.color, NULL);
	}
	else
	{
		sw_data->fill(r, &r->current, x, y, len, data);
	}
#endif
}
