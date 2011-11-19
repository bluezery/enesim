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
/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/
#define MUL4_SYM(x, y) \
 ( ((((((x) >> 16) & 0xff00) * (((y) >> 16) & 0xff00)) + 0xff0000) & 0xff000000) + \
   ((((((x) >> 8) & 0xff00) * (((y) >> 16) & 0xff)) + 0xff00) & 0xff0000) + \
   ((((((x) & 0xff00) * ((y) & 0xff00)) + 0xff00) >> 16) & 0xff00) + \
   (((((x) & 0xff) * ((y) & 0xff)) + 0xff) >> 8) )

#define INTERP_65536(a, c0, c1) \
	( ((((((c0 >> 16) & 0xff00) - ((c1 >> 16) & 0xff00)) * a) + \
	  (c1 & 0xff000000)) & 0xff000000) + \
	  ((((((c0 >> 16) & 0xff) - ((c1 >> 16) & 0xff)) * a) + \
	  (c1 & 0xff0000)) & 0xff0000) + \
	  ((((((c0 & 0xff00) - (c1 & 0xff00)) * a) >> 16) + \
	  (c1 & 0xff00)) & 0xff00) + \
	  ((((((c0 & 0xff) - (c1 & 0xff)) * a) >> 16) + \
	  (c1 & 0xff)) & 0xff) )

#define MUL_A_65536(a, c) \
	( ((((c >> 16) & 0xff00) * a) & 0xff000000) + \
	  ((((c >> 16) & 0xff) * a) & 0xff0000) + \
	  ((((c & 0xff00) * a) >> 16) & 0xff00) + \
	  ((((c & 0xff) * a) >> 16) & 0xff) )

typedef struct _Point2D Point2D;
struct _Point2D
{
	double x, y;
};

typedef struct _Polygon_Vertex Polygon_Vertex;
struct _Polygon_Vertex
{
	Point2D v;
	Polygon_Vertex *next;
};

typedef struct _Polygon_Vector Polygon_Vector;
struct _Polygon_Vector
{
	int xx0, yy0, xx1, yy1;
	int a, b, c;
};

typedef struct _Polygon_Edge Polygon_Edge;
struct _Polygon_Edge
{
	int xx0, yy0, xx1, yy1;
	int e, de;
};

typedef struct _Contour_Polygon Contour_Polygon;
struct _Contour_Polygon
{
	Polygon_Vertex *vertices, *last;
	int nverts;

	Contour_Polygon *next;
};

typedef struct _Enesim_Renderer_Figure
{
	Contour_Polygon *polys, *last;
	int npolys;

	Polygon_Vector *vectors;
	int nvectors;

	// ....  geom_transform?

	int lxx, rxx, tyy, byy;

	Enesim_F16p16_Matrix matrix;
	unsigned char changed :1;
} Enesim_Renderer_Figure;

static inline Enesim_Renderer_Figure * _figure_get(Enesim_Renderer *r)
{
	Enesim_Renderer_Figure *thiz;

	thiz = enesim_renderer_shape_data_get(r);
	return thiz;
}

static void figure_stroke_fill_paint_affine_simple(Enesim_Renderer *r, int x,
		int y, unsigned int len, void *ddata)
{
	Enesim_Renderer_Figure *thiz = _figure_get(r);
	Enesim_Shape_Draw_Mode draw_mode;
	Enesim_Color fcolor;
	Enesim_Color scolor;
	Enesim_Renderer *fpaint;
	int stroke = 0;
	uint32_t *dst = ddata;
	unsigned int *d = dst, *e = d + len;
	Polygon_Edge *edges, *edge;
	Polygon_Vector *v = thiz->vectors;
	int nvectors = thiz->nvectors, n = 0, nedges = 0;
	double ox, oy;

	int axx = thiz->matrix.xx, axy = thiz->matrix.xy, axz =
			thiz->matrix.xz;
	int ayy = thiz->matrix.yy, ayz = thiz->matrix.yz;
	int xx = (axx * x) + (axx >> 1) + (axy * y) + (axy >> 1) + axz - 32768;
	int yy = (ayy * y) + (ayy >> 1) + ayz - 32768;

	enesim_renderer_shape_stroke_color_get(r, &scolor);
	enesim_renderer_shape_fill_color_get(r, &fcolor);
 	enesim_renderer_shape_fill_renderer_get(r, &fpaint);
	enesim_renderer_shape_draw_mode_get(r, &draw_mode);
	enesim_renderer_origin_get(r, &ox, &oy);

	xx -= eina_f16p16_double_from(ox);
	yy -= eina_f16p16_double_from(oy);

	if ((((yy >> 16) + 1) < (thiz->tyy >> 16)) ||
			((yy >> 16) > (1 + (thiz->byy >> 16))))
	{
get_out:
		while (d < e)
			*d++ = 0;
		return;
	}

	if (draw_mode == ENESIM_SHAPE_DRAW_MODE_FILL)
	{
		scolor = fcolor;
		stroke = 0;
		if (fpaint)
		{
			Enesim_Renderer_Sw_Data *sdata;

			sdata = enesim_renderer_backend_data_get(fpaint, ENESIM_BACKEND_SOFTWARE);
			sdata->fill(fpaint, x, y, len, dst);
		}
	}
	if (draw_mode == ENESIM_SHAPE_DRAW_MODE_STROKE_FILL)
	{
		stroke = 1;
		if (fpaint)
		{
			Enesim_Renderer_Sw_Data *sdata;

			sdata = enesim_renderer_backend_data_get(fpaint, ENESIM_BACKEND_SOFTWARE);
			sdata->fill(fpaint, x, y, len, dst);
		}
	}
	if (draw_mode == ENESIM_SHAPE_DRAW_MODE_STROKE)
	{
		fcolor = 0;
		fpaint = NULL;
		stroke = 1;
	}

	edges = alloca(nvectors * sizeof(Polygon_Edge));
	edge = edges;
	while (n < nvectors)
	{
		int xx0 = v->xx0, xx1 = v->xx1;
		int yy0 = v->yy0, yy1 = v->yy1;

		if (xx1 < xx0)
		{
			xx0 = xx1;
			xx1 = v->xx0;
		}
		if (yy1 < yy0)
		{
			yy0 = yy1;
			yy1 = v->yy0;
		}
		if ((((yy + 0xffff)) >= (yy0)) && ((yy) <= ((yy1 + 0xffff))))
		{
			edge->xx0 = xx0;
			edge->xx1 = xx1;
			edge->yy0 = yy0;
			edge->yy1 = yy1;
			edge->de = (v->a * (long long int) axx) >> 16;
			edge->e = ((v->a * (long long int) xx) >> 16) +
					((v->b * (long long int) yy) >> 16) +
					v->c;
			edge++;
			nedges++;
		}
		n++;
		v++;
	}
	if (!nedges)
		goto get_out;

	while (d < e)
	{
		unsigned int p0 = 0;
		int count = 0;
		int a = 0;

		n = 0;
		edge = edges;
		while (n < nedges)
		{
			int ee = edge->e;

			if ((yy >= edge->yy0) && (yy < edge->yy1))
			{
				count++;
				if (ee < 0)
					count -= 2;
			}
			if (ee < 0)
				ee = -ee;

			if ((ee < 65536) && ((xx + 0xffff) >= edge->xx0) && (xx
					<= (0xffff + edge->xx1)))
			{
				if (a < 16384)
					a = 65536 - ee;
				else
					a = (a + (65536 - ee)) / 2;
			}

			edge->e += edge->de;
			edge++;
			n++;
		}

		if (count)
		{
			p0 = fcolor;
			if (fpaint)
			{
				p0 = *d;
				if (fcolor != 0xffffffff)
					p0 = MUL4_SYM(fcolor, p0);
			}

			if (stroke && a)
			{
				unsigned int q0 = p0;

				p0 = scolor;
				if (a < 65536)
					p0 = INTERP_65536(a, p0, q0);
			}
		}
		else if (a)
		{
			p0 = scolor;
			if (fpaint && !stroke)
			{
				p0 = *d;
				if (fcolor != 0xffffffff)
					p0 = MUL4_SYM(fcolor, p0);
			}
			if (a < 65536)
				p0 = MUL_A_65536(a, p0);
		}

		*d++ = p0;
		xx += axx;
	}
}

static void figure_stroke_fill_paint_affine(Enesim_Renderer *r, int x, int y,
		unsigned int len, void *ddata)
{
	Enesim_Renderer_Figure *thiz = _figure_get(r);
	Enesim_Shape_Draw_Mode draw_mode;
	Enesim_Color fcolor;
	Enesim_Color scolor;
	Enesim_Renderer *fpaint;
	int stroke = 0;
	uint32_t *dst = ddata;
	unsigned int *d = dst, *e = d + len;
	Polygon_Edge *edges, *edge;
	Polygon_Vector *v = thiz->vectors;
	int nvectors = thiz->nvectors, n = 0, nedges = 0;
	int y0, y1;

	int axx = thiz->matrix.xx, axy = thiz->matrix.xy, axz =
			thiz->matrix.xz;
	int ayx = thiz->matrix.yx, ayy = thiz->matrix.yy, ayz =
			thiz->matrix.yz;
	int xx = (axx * x) + (axx >> 1) + (axy * y) + (axy >> 1) + axz - 32768;
	int yy = (ayx * x) + (ayx >> 1) + (ayy * y) + (ayy >> 1) + ayz - 32768;

	enesim_renderer_shape_stroke_color_get(r, &scolor);
	enesim_renderer_shape_fill_color_get(r, &fcolor);
 	enesim_renderer_shape_fill_renderer_get(r, &fpaint);
	enesim_renderer_shape_draw_mode_get(r, &draw_mode);

	if (((ayx <= 0) && ((yy >> 16) + 1 < (thiz->tyy >> 16))) || ((ayx >= 0)
			&& ((yy >> 16) > 1 + (thiz->byy >> 16))))
	{
		while (d < e)
			*d++ = 0;
		return;
	}

	len--;
	y0 = yy >> 16;
	y1 = yy + (len * ayx);
	y1 = y1 >> 16;
	if (y1 < y0)
	{
		y0 = y1;
		y1 = yy >> 16;
	}
	edges = alloca(nvectors * sizeof(Polygon_Edge));
	edge = edges;
	while (n < nvectors)
	{
		int xx0, yy0;
		int xx1, yy1;

		xx0 = v->xx0;
		xx1 = v->xx1;
		if (xx1 < xx0)
		{
			xx0 = xx1;
			xx1 = v->xx0;
		}
		yy0 = v->yy0;
		yy1 = v->yy1;
		if (yy1 < yy0)
		{
			yy0 = yy1;
			yy1 = v->yy0;
		}
		if ((y0 <= (yy1 >> 16)) && (y1 >= (yy0 >> 16)))
		{
			edge->xx0 = xx0;
			edge->xx1 = xx1;
			edge->yy0 = yy0;
			edge->yy1 = yy1;
			edge->de = ((v->a * (long long int) axx) >> 16) +
					((v->b * (long long int) ayx) >> 16);
			edge->e = ((v->a * (long long int) xx) >> 16) +
					((v->b * (long long int) yy) >> 16) +
					v->c;
			edge++;
			nedges++;
		}
		n++;
		v++;
	}

	if (draw_mode == ENESIM_SHAPE_DRAW_MODE_FILL)
	{
		scolor = fcolor;
		stroke = 0;
		if (fpaint)
		{
			Enesim_Renderer_Sw_Data *sdata;

			sdata = enesim_renderer_backend_data_get(fpaint, ENESIM_BACKEND_SOFTWARE);
			sdata->fill(fpaint, x, y, len, dst);
		}
	}
	if (draw_mode == ENESIM_SHAPE_DRAW_MODE_STROKE_FILL)
	{
		stroke = 1;
		if (fpaint)
		{
			Enesim_Renderer_Sw_Data *sdata;

			sdata = enesim_renderer_backend_data_get(fpaint, ENESIM_BACKEND_SOFTWARE);
			sdata->fill(fpaint, x, y, len, dst);
		}
	}
	if (draw_mode == ENESIM_SHAPE_DRAW_MODE_STROKE)
	{
		fcolor = 0;
		fpaint = NULL;
		stroke = 1;
	}

	while (d < e)
	{
		unsigned int p0 = 0;
		int count = 0;
		int a = 0;

		n = 0;
		edge = edges;
		while (n < nedges)
		{
			int ee = edge->e;

			if (((yy + 0xffff) >= edge->yy0) &&
					(yy <= (edge->yy1 + 0xffff)))
			{
				if ((yy >= edge->yy0) && (yy < edge->yy1))
				{
					count++;
					if (ee < 0)
						count -= 2;
				}
				if (ee < 0)
					ee = -ee;
				if ((ee < 65536) &&
						((xx + 0xffff) >= edge->xx0) &&
						(xx <= (0xffff + edge->xx1)))
				{
					if (a < 16384)
						a = 65536 - ee;
					else
						a = (a + (65536 - ee)) / 2;
				}
			}

			edge->e += edge->de;
			edge++;
			n++;
		}

		if (count)
		{
			p0 = fcolor;
			if (fpaint)
			{
				p0 = *d;
				if (fcolor != 0xffffffff)
					p0 = MUL4_SYM(fcolor, p0);
			}

			if (stroke && a)
			{
				unsigned int q0 = p0;

				p0 = scolor;
				if (a < 65536)
					p0 = INTERP_65536(a, p0, q0);
			}
		}
		else if (a)
			p0 = MUL_A_65536(a, scolor);

		*d++ = p0;
		yy += ayx;
		xx += axx;
	}
}

static void figure_stroke_fill_paint_proj(Enesim_Renderer *r, int x, int y,
		unsigned int len, void *ddata)
{
	Enesim_Renderer_Figure *thiz = _figure_get(r);
	Enesim_Shape_Draw_Mode draw_mode;
	Enesim_Color fcolor;
	Enesim_Color scolor;
	Enesim_Renderer *fpaint;
	int stroke = 0;
	uint32_t *dst = ddata;
	unsigned int *d = dst, *e = d + len;
	Polygon_Edge *edges, *edge;
	Polygon_Vector *v = thiz->vectors;
	int nvectors = thiz->nvectors, n = 0;

	int axx = thiz->matrix.xx, axy = thiz->matrix.xy, axz =
			thiz->matrix.xz;
	int ayx = thiz->matrix.yx, ayy = thiz->matrix.yy, ayz =
			thiz->matrix.yz;
	int azx = thiz->matrix.zx, azy = thiz->matrix.zy, azz =
			thiz->matrix.zz;
	int xx = (axx * x) + (axx >> 1) + (axy * y) + (axy >> 1) + axz - 32768;
	int yy = (ayx * x) + (ayx >> 1) + (ayy * y) + (ayy >> 1) + ayz - 32768;
	int zz = (azx * x) + (azx >> 1) + (azy * y) + (azy >> 1) + azz;
	
	enesim_renderer_shape_stroke_color_get(r, &scolor);
	enesim_renderer_shape_fill_color_get(r, &fcolor);
 	enesim_renderer_shape_fill_renderer_get(r, &fpaint);
	enesim_renderer_shape_draw_mode_get(r, &draw_mode);

	edges = alloca(nvectors * sizeof(Polygon_Edge));
	edge = edges;
	while (n < nvectors)
	{
		edge->xx0 = v->xx0;
		edge->xx1 = v->xx1;
		if (edge->xx1 < edge->xx0)
		{
			edge->xx0 = edge->xx1;
			edge->xx1 = v->xx0;
		}
		edge->yy0 = v->yy0;
		edge->yy1 = v->yy1;
		if (edge->yy1 < edge->yy0)
		{
			edge->yy0 = edge->yy1;
			edge->yy1 = v->yy0;
		}
		edge->de = ((v->a * (long long int) axx) >> 16) +
				((v->b * (long long int) ayx) >> 16) +
				((v->c * (long long int) azx) >> 16);
		edge->e = ((v->a * (long long int) xx) >> 16) +
				((v->b * (long long int) yy) >> 16) +
				((v->c * (long long int) zz) >> 16);
		n++;
		v++;
		edge++;
	}

	if (draw_mode == ENESIM_SHAPE_DRAW_MODE_FILL)
	{
		scolor = fcolor;
		stroke = 0;
		if (fpaint)
		{
			Enesim_Renderer_Sw_Data *sdata;

			sdata = enesim_renderer_backend_data_get(fpaint, ENESIM_BACKEND_SOFTWARE);
			sdata->fill(fpaint, x, y, len, dst);
		}
	}
	if (draw_mode == ENESIM_SHAPE_DRAW_MODE_STROKE_FILL)
	{
		stroke = 1;
		if (fpaint)
		{
			Enesim_Renderer_Sw_Data *sdata;

			sdata = enesim_renderer_backend_data_get(fpaint, ENESIM_BACKEND_SOFTWARE);
			sdata->fill(fpaint, x, y, len, dst);
		}
	}
	if (draw_mode == ENESIM_SHAPE_DRAW_MODE_STROKE)
	{
		fcolor = 0;
		fpaint = NULL;
		stroke = 1;
	}

	while (d < e)
	{
		unsigned int p0 = 0;
		int count = 0;
		int a = 0;
		int sxx, syy;

		n = 0;
		edge = edges;
		if (zz)
		{
			syy = (((long long int) yy) << 16) / zz;
			sxx = (((long long int) xx) << 16) / zz;

			while (n < nvectors)
			{
				int ee = (((long long int) edge->e) << 16) / zz;

				if (((syy + 0xffff) >= edge->yy0) &&
						(syy <= (edge->yy1 + 0xffff)))
				{
					if ((syy >= edge->yy0) &&
							(syy < edge->yy1))
					{
						count++;
						if (ee < 0)
							count -= 2;
					}
					if (ee < 0)
						ee = -ee;
					if ((ee < 65536) &&
							((sxx + 0xffff) >= edge->xx0) &&
							(sxx <= (0xffff + edge->xx1)))
					{
						if (a < 16384)
							a = 65536 - ee;
						else
							a = (a + (65536 - ee)) / 2;
					}
				}
				edge->e += edge->de;
				edge++;
				n++;
			}
			if (count)
			{
				p0 = fcolor;
				if (fpaint)
				{
					p0 = *d;
					if (fcolor != 0xffffffff)
						p0 = MUL4_SYM(fcolor, p0);
				}

				if (stroke && a)
				{
					unsigned int q0 = p0;

					p0 = scolor;
					if (a < 65536)
						p0 = INTERP_65536(a, p0, q0);
				}
			}
			else if (a)
				p0 = MUL_A_65536(a, scolor);
		}
		*d++ = p0;
		xx += axx;
		yy += ayx;
		zz += azx;
	}
}
/*----------------------------------------------------------------------------*
 *                      The Enesim's renderer interface                       *
 *----------------------------------------------------------------------------*/
static const char * _figure_name(Enesim_Renderer *r)
{
	return "figure";
}

static void _free(Enesim_Renderer *r)
{
	enesim_renderer_figure_clear(r);
}

static Eina_Bool _state_setup(Enesim_Renderer *r,
		const Enesim_Renderer_State *state,
		Enesim_Surface *s,
		Enesim_Renderer_Sw_Fill *fill, Enesim_Error **error)
{
	Enesim_Renderer_Figure *thiz;

	thiz = _figure_get(r);
	if (!thiz)
	{
		return EINA_FALSE;
	}
	if (!thiz->polys)
	{
		return EINA_FALSE;
	}

	if (thiz->changed)
	{
		Contour_Polygon *poly;
		int nvectors = 0;
		Polygon_Vector *vec;

		free(thiz->vectors);
		poly = thiz->polys;
		while (poly)
		{
			if (!poly->vertices || (poly->nverts < 3))
			{
				WRN("Not enough vertices %d", poly->nverts);
				return EINA_FALSE;
			}
			nvectors += poly->nverts;
			if ((poly->last->v.x == poly->vertices->v.x) &&
					(poly->last->v.y == poly->vertices->v.y))
				nvectors--;
			poly = poly->next;
		}

		thiz->vectors = calloc(nvectors, sizeof(Polygon_Vector));
		if (!thiz->vectors)
		{
			return EINA_FALSE;
		}

		thiz->nvectors = nvectors;
		poly = thiz->polys;
		vec = thiz->vectors;
		thiz->lxx = 65536;
		thiz->rxx = -65536;
		thiz->tyy = 65536;
		thiz->byy = -65536;
		while (poly)
		{
			Polygon_Vertex *v, *nv;
			double x0, y0, x1, y1;
			double x01, y01;
			double len;
			int n = 0, nverts = poly->nverts;

			if ((poly->last->v.x == poly->vertices->v.x) &&
					(poly->last->v.y == poly->vertices->v.y))
				nverts--;
			v = poly->vertices;
			while (n < nverts)
			{
				nv = v->next;
				if (n == (poly->nverts - 1))
					nv = poly->vertices;
				x0 = v->v.x;
				y0 = v->v.y;
				x1 = nv->v.x;
				y1 = nv->v.y;
				x0 = ((int) (x0 * 256)) / 256.0;
				x1 = ((int) (x1 * 256)) / 256.0;
				y0 = ((int) (y0 * 256)) / 256.0;
				y1 = ((int) (y1 * 256)) / 256.0;
				x01 = x1 - x0;
				y01 = y1 - y0;
				if ((len = hypot(x01, y01)) < (1 / 256.0))
					return 0;
				len *= 1 + (1 / 16.0);
				vec->a = -(y01 * 65536) / len;
				vec->b = (x01 * 65536) / len;
				vec->c = (65536 * ((y1 * x0) - (x1 * y0)))
						/ len;
				vec->xx0 = x0 * 65536;
				vec->yy0 = y0 * 65536;
				vec->xx1 = x1 * 65536;
				vec->yy1 = y1 * 65536;

				if (vec->yy0 < thiz->tyy)
					thiz->tyy = vec->yy0;
				if (vec->yy0 > thiz->byy)
					thiz->byy = vec->yy0;

				if (vec->xx0 < thiz->lxx)
					thiz->lxx = vec->xx0;
				if (vec->xx0 > thiz->rxx)
					thiz->rxx = vec->xx0;

				n++;
				vec++;
				v = nv;
			}
			poly = poly->next;
		}
		thiz->changed = 0;
	}

	if (!enesim_renderer_shape_setup(r, state, s, error))
	{
		return EINA_FALSE;
	}

	enesim_matrix_f16p16_matrix_to(&state->transformation,
			&thiz->matrix);

	*fill = figure_stroke_fill_paint_proj;
	if (state->transformation_type != ENESIM_MATRIX_PROJECTIVE)
	{
		*fill = figure_stroke_fill_paint_affine;
		if (&state->transformation.yx == 0)
			*fill = figure_stroke_fill_paint_affine_simple;
	}

	return EINA_TRUE;
}

static void _state_cleanup(Enesim_Renderer *r, Enesim_Surface *s)
{
	enesim_renderer_shape_cleanup(r, s);

	/*
	 if (thiz->stroke.rend &&
			((thiz->draw_mode == ENESIM_SHAPE_DRAW_MODE_STROKE) ||
			(thiz->draw_mode == ENESIM_SHAPE_DRAW_MODE_STROKE_FILL)))
		 enesim_renderer_sw_cleanup(thiz->stroke.paint);
	 */
}

static void _figure_boundings(Enesim_Renderer *r, Enesim_Rectangle *boundings)
{
	Enesim_Renderer_Figure *thiz;

	thiz = _figure_get(r);

	/* FIXME split the setp on two functions */
	if (!enesim_renderer_sw_setup(r, NULL, NULL, NULL))
		return;
	boundings->x = (thiz->lxx - 0xffff) >> 16;
	boundings->y = (thiz->tyy - 0xffff) >> 16;
	boundings->w = ((thiz->rxx - 0xffff) >> 16) - boundings->x + 1;
	boundings->h = ((thiz->byy - 0xffff) >> 16) - boundings->y + 1;
}

static void _figure_flags(Enesim_Renderer *r, Enesim_Renderer_Flag *flags)
{
	*flags = ENESIM_RENDERER_FLAG_AFFINE |
			ENESIM_RENDERER_FLAG_PROJECTIVE |
			ENESIM_RENDERER_FLAG_ARGB8888;
}

static Enesim_Renderer_Descriptor _figure_descriptor = {
	/* .version = 			*/ ENESIM_RENDERER_API,
	/* .name = 			*/ _figure_name,
	/* .free = 			*/ _free,
	/* .boundings = 		*/ _figure_boundings,
	/* .destination_transform = 	*/ NULL,
	/* .flags = 			*/ _figure_flags,
	/* .is_inside = 		*/ NULL,
	/* .damage = 			*/ NULL,
	/* .has_changed = 		*/ NULL,
	/* .sw_setup = 			*/ _state_setup,
	/* .sw_cleanup = 		*/ _state_cleanup
};
/*============================================================================*
 *                                 Global                                     *
 *============================================================================*/
/*============================================================================*
 *                                   API                                      *
 *============================================================================*/
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI Enesim_Renderer * enesim_renderer_figure_new(void)
{
	Enesim_Renderer *r;
	Enesim_Renderer_Figure *thiz;

	thiz = calloc(1, sizeof(Enesim_Renderer_Figure));
	if (!thiz) return NULL;
	r = enesim_renderer_shape_new(&_figure_descriptor, thiz);
	return r;
}
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_figure_polygon_add(Enesim_Renderer *r)
{
	Enesim_Renderer_Figure *thiz;
	Contour_Polygon *poly;

	thiz = _figure_get(r);

	poly = calloc(1, sizeof(Contour_Polygon));
	if (!poly)
		return;

	if (!thiz->polys)
	{
		thiz->polys = thiz->last = poly;
		thiz->npolys++;
		thiz->changed = 1;
		return;
	}
	thiz->last->next = poly;
	thiz->last = poly;
	thiz->npolys++;
	thiz->changed = 1;
}
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_figure_polygon_vertex_add(Enesim_Renderer *r,
		double x, double y)
{
	Enesim_Renderer_Figure *thiz;
	Contour_Polygon *poly;
	Polygon_Vertex *vertex;

	thiz = _figure_get(r);

	if (!thiz->polys)
		return; // maybe just add one instead
	poly = thiz->last;
	if (poly->last &&
			(fabs(poly->last->v.x - x) < (1 / 256.0)) &&
			(fabs(poly->last->v.y - y) < (1 / 256.0)))
		return;
	vertex = calloc(1, sizeof(Polygon_Vertex));
	if (!vertex)
		return;
	vertex->v.x = x;
	vertex->v.y = y;

	if (!poly->vertices)
	{
		poly->vertices = poly->last = vertex;
		poly->nverts++;
		thiz->changed = 1;
		return;
	}
	poly->last->next = vertex;
	poly->last = vertex;
	poly->nverts++;
	thiz->changed = 1;
}
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_figure_clear(Enesim_Renderer *r)
{
	Enesim_Renderer_Figure *thiz;
	Contour_Polygon *c, *nc;
	Polygon_Vertex *v, *nv;

	thiz = _figure_get(r);
	if (!thiz->polys)
		return;
	c = thiz->polys;
	while (c)
	{
		v = c->vertices;
		while (v)
		{
			nv = v->next;
			free(v);
			v = nv;
		}
		nc = c->next;
		free(c);
		c = nc;
	}
	thiz->polys = thiz->last = NULL;
	thiz->npolys = 0;
	free(thiz->vectors);
	thiz->vectors = NULL;
	thiz->changed = 1;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_figure_polygon_set(Enesim_Renderer *r, Eina_List *list)
{
	Enesim_Renderer_Figure_Polygon *polygon;
	Eina_List *l1;
	Enesim_Renderer_Figure *thiz;

	thiz = _figure_get(r);
	enesim_renderer_figure_clear(r);
	EINA_LIST_FOREACH(list, l1, polygon)
	{
		Eina_List *l2;
		Enesim_Renderer_Figure_Vertex *vertex;

		enesim_renderer_figure_polygon_add(r);
		EINA_LIST_FOREACH(polygon->vertices, l2, vertex)
		{
			enesim_renderer_figure_polygon_vertex_add(r, vertex->x, vertex->y);
		}
	}
}


#if 0
int
paint_is_polygon(Enesim_Renderer *r)
{
	Enesim_Renderer_Shape *thiz;

	if (!r || !r->type) return 0;
	if (r->type->id != SHAPE_PAINT) return 0;
	thiz = (Enesim_Renderer_Shape *)r;
	if (!thiz || (thiz->id != POLYGON_PAINT)) return 0;
	return 1;
}
#endif

