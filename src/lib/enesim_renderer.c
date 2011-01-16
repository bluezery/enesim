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
/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/
#define ENESIM_MAGIC_RENDERER 0xe7e51402
#define ENESIM_MAGIC_CHECK_RENDERER(d)\
	do {\
		if (!EINA_MAGIC_CHECK(d, ENESIM_MAGIC_RENDERER))\
			EINA_MAGIC_FAIL(d, ENESIM_MAGIC_RENDERER);\
	} while(0)
/*============================================================================*
 *                                 Global                                     *
 *============================================================================*/
void enesim_renderer_init(Enesim_Renderer *r)
{
	EINA_MAGIC_SET(r, ENESIM_MAGIC_RENDERER);
	/* common properties */
	r->ox = 0;
	r->oy = 0;
	r->color = ENESIM_COLOR_FULL;
	r->rop = ENESIM_FILL;
	enesim_f16p16_matrix_identity(&r->matrix.values);
	enesim_matrix_identity(&r->matrix.original);
	r->matrix.type = ENESIM_MATRIX_IDENTITY;
}

void enesim_renderer_relative_set(Enesim_Renderer *r, Enesim_Renderer *rel,
		Enesim_Matrix *old_matrix, double *old_ox, double *old_oy)
{
	Enesim_Matrix rel_matrix, r_matrix;
	double r_ox, r_oy;
	float nox, noy;

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
	/* FIXME what to do with the origin? */
#if 0
	enesim_matrix_point_transform(old_matrix, *old_ox + r_ox, *old_oy + r_oy, &nox, &noy);
	enesim_renderer_origin_set(rel, nox, noy);
#else
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
		*descriptor, Enesim_Renderer_Flag flags, void *data)
{
	Enesim_Renderer *r;

	r = malloc(sizeof(Enesim_Renderer));
	enesim_renderer_init(r);
	r->boundings = descriptor->boundings;
	r->sw_setup = descriptor->sw_setup;
	r->sw_cleanup = descriptor->sw_cleanup;
	r->free = descriptor->free;
	r->flags = flags;
	r->data = data;

	return r;
}
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI Eina_Bool enesim_renderer_sw_setup(Enesim_Renderer *r)
{
	Enesim_Renderer_Sw_Fill fill;
	Eina_Bool ret;

	ENESIM_MAGIC_CHECK_RENDERER(r);
	if (!r->sw_setup) return EINA_TRUE;
	if (r->sw_setup(r, &fill))
	{
		r->sw_fill = fill;
		return EINA_TRUE;
	}
	return EINA_FALSE;
}
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_sw_cleanup(Enesim_Renderer *r)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	if (r->sw_cleanup) r->sw_cleanup(r);
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
	//enesim_renderer_sw_cleanup(r);
	if (r->free)
		r->free(r);
	free(r);
}
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_flags(Enesim_Renderer *r, Enesim_Renderer_Flag *flags)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	*flags = r->flags;
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
	if (color) color = r->color;
}

/**
 * Gets the bounding box of the renderer on its own coordinate space
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_boundings(Enesim_Renderer *r, Eina_Rectangle *rect)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	if (rect && r->boundings) r->boundings(r, rect);
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

	ENESIM_MAGIC_CHECK_RENDERER(r);
	if (rect && r->boundings)
	{
		r->boundings(r, rect);
		rect->x += lround(r->ox);
		rect->y += lround(r->oy);
		if (r->matrix.type != ENESIM_MATRIX_IDENTITY)
		{
			Enesim_Quad q;
			Enesim_Matrix m;

			enesim_matrix_inverse(&r->matrix.original, &m);
			enesim_matrix_rect_transform(&m, rect, &q);
			enesim_quad_rectangle_to(&q, rect);
		}
		rect->x -= x;
		rect->y -= y;
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
 * To be documented
 * FIXME: To be fixed
 * What about the mask?
 */
EAPI void enesim_renderer_surface_draw(Enesim_Renderer *r, Enesim_Surface *s,
		Eina_Rectangle *clip, int x, int y)
{
	Enesim_Compositor_Span span;
	Enesim_Renderer_Sw_Fill fill;
	Eina_Rectangle boundings;
	Eina_Rectangle final;
	uint32_t *ddata;
	int stride;
	Enesim_Format dfmt;

	ENESIM_MAGIC_CHECK_RENDERER(r);
	ENESIM_MAGIC_CHECK_SURFACE(s);

	if (!enesim_renderer_sw_setup(r)) return;
	fill = enesim_renderer_sw_fill_get(r);
	if (!fill) return;

	if (!clip)
	{
		final.x = 0;
		final.y = 0;
		enesim_surface_size_get(s, &final.w, &final.h);
	}
	else
	{
		final.x = clip->x;
		final.y = clip->y;
		final.w = clip->w;
		final.h = clip->h;
	}
	/* clip against the destination rectangle */
	enesim_renderer_destination_boundings(r, &boundings, 0, 0);
	if (!eina_rectangle_intersection(&final, &boundings))
	{
		printf("destination rectangle does not intersect\n");
		return;
	}
	dfmt = enesim_surface_format_get(s);
	ddata = enesim_surface_data_get(s);
	stride = enesim_surface_stride_get(s);
	ddata = ddata + (final.y * stride) + final.x;

	/* translate the origin */
	final.x -= x;
	final.y -= y;
	/* fill the new span */
	if ((r->rop == ENESIM_FILL) && (r->color == ENESIM_COLOR_FULL))
	{
		while (final.h--)
		{
			fill(r, final.x, final.y, final.w, ddata);
			final.y++;
			ddata += stride;
		}
	}
	else
	{
		uint32_t *fdata;

		span = enesim_compositor_span_get(r->rop, &dfmt, ENESIM_FORMAT_ARGB8888,
				r->color, ENESIM_FORMAT_NONE);

		fdata = alloca(final.w * sizeof(uint32_t));
		while (final.h--)
		{
			fill(r, final.x, final.y, final.w, fdata);
			final.y++;
			/* compose the filled and the destination spans */
			span(ddata, final.w, fdata, r->color, NULL);
			ddata += stride;
		}
	}
	/* TODO set the format again */
	enesim_renderer_sw_cleanup(r);
}

/**
 *
 */
EAPI void enesim_renderer_rop_set(Enesim_Renderer *r, Enesim_Rop rop)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	r->rop = rop;
}

/**
 *
 */
EAPI void enesim_renderer_rop_get(Enesim_Renderer *r, Enesim_Rop *rop)
{
	ENESIM_MAGIC_CHECK_RENDERER(r);
	if (rop) *rop = r->rop;
}
