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
#include "float.h"
/* TODO whenever we append/prepend a new point, calculate the boundings */
/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/
static inline void _polygon_update_bounds(Enesim_Polygon *ep, Enesim_Point *p)
{
	if (p->x > ep->xmax) ep->xmax = p->x;
	if (p->y > ep->ymax) ep->ymax = p->y;
	if (p->x < ep->xmin) ep->xmin = p->x;
	if (p->y < ep->ymin) ep->ymin = p->y;
}

static Eina_Bool _points_equal(Enesim_Point *p0, Enesim_Point *p1, double threshold)
{
	double x01;
	double y01;

	x01 = fabs(p0->x - p1->x);
	y01 = fabs(p0->y - p1->y);
	if (x01 < threshold && y01 < threshold)
		return EINA_TRUE;
	return EINA_FALSE;
}
/*============================================================================*
 *                                 Global                                     *
 *============================================================================*/
Enesim_Point * enesim_point_new(void)
{
	Enesim_Point *thiz;

	thiz = calloc(1, sizeof(Enesim_Point));
	return thiz;
}

Enesim_Point * enesim_point_new_from_coords(double x, double y)
{
	Enesim_Point *thiz;

	thiz = enesim_point_new();
	thiz->x = x;
	thiz->y = y;

	return thiz;
}

Enesim_Polygon * enesim_polygon_new(void)
{
	Enesim_Polygon *p;

	p = calloc(1, sizeof(Enesim_Polygon));
	p->xmax = p->ymax = -DBL_MAX;
	p->xmin = p->ymin = DBL_MAX;
	p->threshold = DBL_EPSILON;
	return p;
}

void enesim_polygon_threshold_set(Enesim_Polygon *p, double threshold)
{
	p->threshold = threshold;
}

void enesim_polygon_point_append_from_coords(Enesim_Polygon *thiz, double x, double y)
{
	Enesim_Point *p;

	p = enesim_point_new_from_coords(x, y);
	enesim_polygon_point_append(thiz, p);
}

void enesim_polygon_point_append(Enesim_Polygon *thiz, Enesim_Point *p)
{
	/* FIXME check the threshold */
	thiz->points = eina_list_append(thiz->points, p);
	_polygon_update_bounds(thiz, p);
}

void enesim_polygon_point_prepend_from_coords(Enesim_Polygon *thiz, double x, double y)
{
	Enesim_Point *p;

	p = enesim_point_new_from_coords(x, y);
	enesim_polygon_point_prepend(thiz, p);
}

void enesim_polygon_point_prepend(Enesim_Polygon *thiz, Enesim_Point *p)
{
	/* FIXME check the threshold */
	thiz->points = eina_list_prepend(thiz->points, p);
	_polygon_update_bounds(thiz, p);
}

int enesim_polygon_point_count(Enesim_Polygon *thiz)
{
	return eina_list_count(thiz->points);
}

void enesim_polygon_clear(Enesim_Polygon *thiz)
{
	Enesim_Point *pt;
	EINA_LIST_FREE(thiz->points, pt)
	{
		free(pt);
	}
}

void enesim_polygon_delete(Enesim_Polygon *thiz)
{
	Enesim_Point *pt;
	EINA_LIST_FREE(thiz->points, pt)
		free(pt);
	free(thiz);
}

void enesim_polygon_merge(Enesim_Polygon *thiz, Enesim_Polygon *to_merge)
{
	Enesim_Point *last, *first;
	Eina_List *l;
	
	/* check that the last point at thiz is not equal to the first point to merge */
	l = eina_list_last(thiz->points);
	last = eina_list_data_get(l);
	first = eina_list_data_get(to_merge->points);
	if (_points_equal(first, last, thiz->threshold))
	{
		to_merge->points = eina_list_remove(to_merge->points, first);
		free(free);
	}
	thiz->points = eina_list_merge(thiz->points, to_merge->points);
}

void enesim_polygon_close(Enesim_Polygon *thiz, Eina_Bool close)
{
	thiz->closed = close;
}

void enesim_polygon_boundings(Enesim_Polygon *ep, Enesim_Rectangle *bounds)
{

}

Enesim_Figure * enesim_figure_new(void)
{
	Enesim_Figure *thiz;

	thiz = calloc(1, sizeof(Enesim_Figure));
	return thiz;
}

void enesim_figure_delete(Enesim_Figure *thiz)
{
	enesim_figure_clear(thiz);
	free(thiz);
}

void enesim_figure_polygon_append(Enesim_Figure *thiz, Enesim_Polygon *p)
{
	thiz->polygons = eina_list_append(thiz->polygons, p);
}

void enesim_figure_polygon_remove(Enesim_Figure *thiz, Enesim_Polygon *p)
{
	thiz->polygons = eina_list_remove(thiz->polygons, p);
}

int enesim_figure_polygon_count(Enesim_Figure *thiz)
{
	return eina_list_count(thiz->polygons);
}

void enesim_figure_clear(Enesim_Figure *thiz)
{
	Enesim_Polygon *p;
	EINA_LIST_FREE(thiz->polygons, p)
	{
		enesim_polygon_delete(p);
	}
}

void enesim_figure_dump(Enesim_Figure *f)
{
	Enesim_Polygon *p;
	Eina_List *l1;

	EINA_LIST_FOREACH(f->polygons, l1, p)
	{
		Enesim_Point *pt;
		Eina_List *l2;

		printf("New %s polygon\n", p->closed ? "closed": "");
		EINA_LIST_FOREACH(p->points, l2, pt)
		{
			printf("%g %g\n", pt->x, pt->y);
		}
	}
}
/*============================================================================*
 *                                   API                                      *
 *============================================================================*/

