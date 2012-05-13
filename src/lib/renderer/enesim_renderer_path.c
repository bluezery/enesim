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
#include "private/shape.h"
#include "private/rasterizer.h"
/**
 * TODO
 * - Use the threshold on the curve state
 */
/*
 * Some formulas found on the research process:
 * l1 = Ax + By + C
 * l2 || l1 => l2 = Ax + By + C'
 * C' = C + d * hypot(A, B);
 */

/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/
#define ENESIM_RENDERER_PATH_MAGIC_CHECK(d) \
	do {\
		if (!EINA_MAGIC_CHECK(d, ENESIM_RENDERER_PATH_MAGIC))\
			EINA_MAGIC_FAIL(d, ENESIM_RENDERER_PATH_MAGIC);\
	} while(0)

typedef void (*Enesim_Renderer_Path_Polygon_Add)(void *data);
typedef void (*Enesim_Renderer_Path_Polygon_Close)(Eina_Bool close, void *data);
typedef void (*Enesim_Renderer_Path_Done)(void *data);

typedef struct _Enesim_Renderer_Command_State
{
	Enesim_Curve_State st;
	Enesim_Curve_Vertex_Add vertex_add;
	Enesim_Renderer_Path_Polygon_Add polygon_add;
	Enesim_Renderer_Path_Polygon_Close polygon_close;
	Enesim_Renderer_Path_Done path_done;
	void *data;
} Enesim_Renderer_Command_State;

typedef struct _Enesim_Renderer_Path_Strokeless_State
{
	Enesim_Figure *fill_figure;
} Enesim_Renderer_Path_Strokeless_State;

typedef struct _Enesim_Renderer_Path_Stroke_State
{
	Enesim_Polygon *original_polygon;
	Enesim_Polygon *offset_polygon;
	Enesim_Polygon *inset_polygon;
	Enesim_Figure *fill_figure;
	Enesim_Figure *stroke_figure;
	Enesim_Point first;
	Enesim_Point p0, p1, p2;
	Enesim_Point n01, n12;
	Enesim_Shape_Stroke_Join join;
	Enesim_Shape_Stroke_Cap cap;
	double rx;
	double ry;
	int count;
} Enesim_Renderer_Path_Stroke_State;

typedef struct _Enesim_Renderer_Path
{
	EINA_MAGIC
	/* properties */
	Eina_List *commands;
	/* private */
	Enesim_Figure *fill_figure;
	Enesim_Figure *stroke_figure;

	Enesim_Renderer *bifigure;
	Eina_Bool changed : 1;
	Eina_Bool needs_setup : 1; /* FIXME the changed is not enough */
} Enesim_Renderer_Path;

static inline Enesim_Renderer_Path * _path_get(Enesim_Renderer *r)
{
	Enesim_Renderer_Path *thiz;

	thiz = enesim_renderer_shape_data_get(r);
	ENESIM_RENDERER_PATH_MAGIC_CHECK(thiz);

	return thiz;
}
/*----------------------------------------------------------------------------*
 *                                With stroke                                 *
 *----------------------------------------------------------------------------*/
typedef struct _Enesim_Path_Edge
{
	double x0;
	double y0;
	double x1;
	double y1;
} Enesim_Path_Edge;

static void _edge_join(Enesim_Path_Edge *e1,
		Enesim_Path_Edge *e2,
		Enesim_Shape_Stroke_Join join,
		double rx,
		double ry,
		Eina_Bool large,
		Eina_Bool sweep,
		Enesim_Curve_Vertex_Add vertex_add,
		void *data)
{
	Enesim_Curve_State st;
	switch (join)
	{
		/* TODO here we should add the intersection of both edges */
		/* we need a miter length variable too */
		case ENESIM_JOIN_MITER:
		vertex_add(e2->x0, e2->y0, data);
		break;

		/* join theme with an arc */
		case ENESIM_JOIN_ROUND:
		st.vertex_add = vertex_add;
		st.last_x = e1->x1;
		st.last_y = e1->y1;
		st.last_ctrl_x = e1->x1;
		st.last_ctrl_y = e1->y1;
		st.data = data;
		enesim_curve_arc_to(&st, rx, ry, 0, large, sweep, e2->x0, e2->y0);
		break;

		case ENESIM_JOIN_BEVEL:
		/* just join theme with a line */
		vertex_add(e2->x0, e2->y0, data);
		break;

		default:
		break;
	}
}

static inline Eina_Bool _do_normal(Enesim_Point *n, Enesim_Point *p0, Enesim_Point *p1, double threshold)
{
	double dx;
	double dy;
	double f;

	dx = p1->x - p0->x;
	dy = p1->y - p0->y;

	if (fabs(dx) < threshold && fabs(dy) < threshold)
		return EINA_FALSE;
	f = 1.0 / hypot(dx, dy);
	n->x = dy * f;
	n->y = -dx * f;

	return EINA_TRUE;
}

static void _stroke_curve_append(double x, double y, void *data)
{
	Enesim_Polygon *p = data;

	enesim_polygon_point_append_from_coords(p, x, y);
}

static void _stroke_curve_prepend(double x, double y, void *data)
{
	Enesim_Polygon *p = data;

	enesim_polygon_point_prepend_from_coords(p, x, y);
}

static void _stroke_path_vertex_process(double x, double y, void *data)
{
	Enesim_Renderer_Path_Stroke_State *thiz = data;
	Enesim_Polygon *inset = thiz->inset_polygon;
	Enesim_Polygon *offset = thiz->offset_polygon;
	Enesim_Point o0, o1;
	Enesim_Point i0, i1;
	Eina_Bool large;
	Enesim_Path_Edge e1, e2;
	double ox;
	double oy;
	int c1;
	int c2;

	/* just store the first point */
	if (thiz->count < 2)
	{
		switch (thiz->count)
		{
			case 0:
			thiz->first.x = thiz->p0.x = x;
			thiz->first.y = thiz->p0.y = y;
			thiz->count++;
			return;

			case 1:
			thiz->p1.x = x;
			thiz->p1.y = y;
			/* FIXME use the threshold set by the quality prop or something like that */
			if (!_do_normal(&thiz->n01, &thiz->p0, &thiz->p1, 1/256.0))
				return;

			ox = thiz->rx  * thiz->n01.x;
			oy = thiz->ry * thiz->n01.y;

			o0.x = thiz->p0.x + ox;
			o0.y = thiz->p0.y + oy;
			enesim_polygon_point_append_from_coords(offset, o0.x, o0.y);

			o1.x = thiz->p1.x + ox;
			o1.y = thiz->p1.y + oy;
			enesim_polygon_point_append_from_coords(offset, o1.x, o1.y);

			i0.x = thiz->p0.x - ox;
			i0.y = thiz->p0.y - oy;
			enesim_polygon_point_prepend_from_coords(inset, i0.x, i0.y);

			i1.x = thiz->p1.x - ox;
			i1.y = thiz->p1.y - oy;
			enesim_polygon_point_prepend_from_coords(inset, i1.x, i1.y);

			thiz->count++;
			return;

			default:
			break;
		}
	}

	/* get the normals of the new edge */
	thiz->p2.x = x;
	thiz->p2.y = y;
	/* FIXME use the threshold set by the quality prop or something like that */
	if (!_do_normal(&thiz->n12, &thiz->p1, &thiz->p2, 1/256.0))
		return;
	ox = thiz->rx * thiz->n12.x;
	oy = thiz->ry * thiz->n12.y;

	o0.x = thiz->p1.x + ox;
	o0.y = thiz->p1.y + oy;

	i0.x = thiz->p1.x - ox;
	i0.y = thiz->p1.y - oy;

	o1.x = thiz->p2.x + ox;
	o1.y = thiz->p2.y + oy;

	i1.x = thiz->p2.x - ox;
	i1.y = thiz->p2.y - oy;

	c1 = ((thiz->p2.x - thiz->p1.x) * thiz->n01.x) + ((thiz->p2.y - thiz->p1.y) * thiz->n01.y);
	c2 = (thiz->n01.x * thiz->n12.x) + (thiz->n01.y * thiz->n12.y);
	/* add the vertices of the new edge */
	/* check if the previous edge and this one to see the concave/convex thing */
	/* dot product
	 * = 1 pointing same direction
	 * > 0 concave
	 * = 0 orthogonal
	 * < 0 convex
	 * = -1 pointing opposite direction
	 */
	if (c2 > 0)
		large = EINA_TRUE;
	else
		large = EINA_FALSE;

	/* right side */
	if (c1 >= 0)
	{
		Enesim_Point *p;
		Eina_List *l2;

		enesim_polygon_point_append_from_coords(offset, o0.x, o0.y);
		/* join the inset */
		p = eina_list_data_get(inset->points);
		e1.x1 = p->x;
		e1.y1 = p->y;
		l2 = eina_list_next(inset->points);
		p = eina_list_data_get(l2);
		e1.x0 = p->x;
		e1.y0 = p->y;

		e2.x0 = i0.x;
		e2.y0 = i0.y;
		e2.x1 = i1.x;
		e2.y1 = i1.y;

		_edge_join(&e1, &e2, thiz->join, thiz->rx, thiz->ry, large, EINA_FALSE,
				_stroke_curve_prepend, inset);
	}
	/* left side */
	else
	{
		Enesim_Point *p;
		Eina_List *l2;

		enesim_polygon_point_prepend_from_coords(inset, i0.x, i0.y);
		/* join the offset */
		l2 = eina_list_last(offset->points);
		p = eina_list_data_get(l2);
		e1.x1 = p->x;
		e1.y1 = p->y;
		l2 = eina_list_prev(l2);
		p = eina_list_data_get(l2);
		e1.x0 = p->x;
		e1.y0 = p->y;

		e2.x0 = o0.x;
		e2.y0 = o0.y;
		e2.x1 = o1.x;
		e2.y1 = o1.y;
		_edge_join(&e1, &e2, thiz->join, thiz->rx, thiz->ry, large, EINA_TRUE,
				_stroke_curve_append, offset);
	}

	enesim_polygon_point_append_from_coords(offset, o1.x, o1.y);
	enesim_polygon_point_prepend_from_coords(inset, i1.x, i1.y);

	thiz->p0 = thiz->p1;
	thiz->p1 = thiz->p2;
	thiz->n01 = thiz->n12;
	thiz->count++;
}

static void _stroke_path_vertex_add(double x, double y, void *data)
{
	Enesim_Renderer_Path_Stroke_State *thiz = data;

	enesim_polygon_point_append_from_coords(thiz->original_polygon, x, y);
	_stroke_path_vertex_process(x, y, data);
}

static void _stroke_path_polygon_add(void *data)
{
        Enesim_Renderer_Path_Stroke_State *thiz = data;
        Enesim_Polygon *p;

        /* just reset */
        thiz->count = 0;

	p = enesim_polygon_new();
	enesim_polygon_threshold_set(p, 1/256.0); // FIXME make 1/256.0 a constant */
	enesim_figure_polygon_append(thiz->fill_figure, p);
	thiz->original_polygon = p;

	p = enesim_polygon_new();
	enesim_polygon_threshold_set(p, 1/256.0);
	enesim_figure_polygon_append(thiz->stroke_figure, p);
	thiz->offset_polygon = p;

	p = enesim_polygon_new();
	enesim_polygon_threshold_set(p, 1/256.0);
	enesim_figure_polygon_append(thiz->stroke_figure, p);
	thiz->inset_polygon = p;
}

static void _stroke_path_done(void *data)
{
        Enesim_Renderer_Path_Stroke_State *thiz = data;

	/* check that we are not closed */
	if (!thiz->original_polygon->closed)
	{
		Enesim_Polygon *to_merge;
		Enesim_Point *off, *ofl;
		Enesim_Point *inf, *inl;

		enesim_polygon_close(thiz->original_polygon, EINA_TRUE);
		/* FIXME is not complete yet */
		/* TODO use the stroke cap to close the offset and the inset */
		if (thiz->cap != ENESIM_CAP_BUTT)
		{
			Eina_List *l;

			inf = eina_list_data_get(thiz->inset_polygon->points);
			l = eina_list_last(thiz->inset_polygon->points);
			inl = eina_list_data_get(l);

			off = eina_list_data_get(thiz->offset_polygon->points);
			l = eina_list_last(thiz->offset_polygon->points);
			ofl = eina_list_data_get(l);
			/* do an arc from last offet to first inset */
			if (thiz->cap == ENESIM_CAP_ROUND)
			{
				Enesim_Curve_State st;

				st.vertex_add = _stroke_curve_prepend;
				st.data = thiz->offset_polygon;
				st.last_x = off->x;
				st.last_y = off->y;
				st.last_ctrl_x = off->x;
				st.last_ctrl_y = off->y;
				/* FIXME what about the sweep and the large? */
				enesim_curve_arc_to(&st, thiz->rx, thiz->ry, 0, EINA_TRUE, EINA_FALSE, inl->x, inl->y);

				st.vertex_add = _stroke_curve_append;
				st.data = thiz->offset_polygon;
				st.last_x = ofl->x;
				st.last_y = ofl->y;
				st.last_ctrl_x = ofl->x;
				st.last_ctrl_y = ofl->y;
				enesim_curve_arc_to(&st, thiz->rx, thiz->ry, 0, EINA_FALSE, EINA_TRUE, inf->x, inf->y);
			}
			/* square case extend the last offset r length and the first inset r length, join them */
			else
			{
				/* TODO */
			}
		}
		to_merge = thiz->inset_polygon;
		enesim_figure_polygon_remove(thiz->stroke_figure, to_merge);
		enesim_polygon_merge(thiz->offset_polygon, thiz->inset_polygon);
	}
	enesim_polygon_close(thiz->offset_polygon, EINA_TRUE);
	enesim_polygon_close(thiz->inset_polygon, EINA_TRUE);
}

static void _stroke_path_polygon_close(Eina_Bool close, void *data)
{
        Enesim_Renderer_Path_Stroke_State *thiz = data;

	if (close)
	{
		Enesim_Point *p;
		Eina_List *l;
		/* also close the figure itself */
		_stroke_path_vertex_process(thiz->first.x, thiz->first.y, thiz);
		enesim_polygon_close(thiz->original_polygon, EINA_TRUE);
		/* close the inset/off with the join cap */
		l = eina_list_next(thiz->original_polygon->points);
		p = eina_list_data_get(l);
		_stroke_path_vertex_process(p->x, p->y, thiz);
	}
}
/*----------------------------------------------------------------------------*
 *                              Without stroke                                *
 *----------------------------------------------------------------------------*/
static void _strokeless_path_vertex_add(double x, double y, void *data)
{
	Enesim_Renderer_Path_Strokeless_State *thiz = data;
	Enesim_Polygon *p;
	Eina_List *last;

	last = eina_list_last(thiz->fill_figure->polygons);
	p = eina_list_data_get(last);
	enesim_polygon_point_append_from_coords(p, x, y);
}

static void _strokeless_path_polygon_add(void *data)
{
	Enesim_Renderer_Path_Strokeless_State *thiz = data;
	Enesim_Polygon *p;

	p = enesim_polygon_new();
	enesim_polygon_threshold_set(p, 1/256.0); // FIXME make 1/256.0 a constant */
	enesim_figure_polygon_append(thiz->fill_figure, p);
}

static void _strokeless_path_polygon_close(Eina_Bool close, void *data)
{
	Enesim_Renderer_Path_Strokeless_State *thiz = data;
	Enesim_Polygon *p;
	Eina_List *last;

	last = eina_list_last(thiz->fill_figure->polygons);
	p = eina_list_data_get(last);
	p->closed = close;
}
/*----------------------------------------------------------------------------*
 *                                 Commands                                   *
 *----------------------------------------------------------------------------*/
static void _move_to(Enesim_Renderer_Command_State *state,
		double x, double y)
{
	/* we have to reset the curve state too */
	state->st.last_x = x;
	state->st.last_y = y;
	state->st.last_ctrl_x = x;
	state->st.last_ctrl_y = y;
	/* now call the interface */
	state->polygon_add(state->data);
	state->vertex_add(x, y, state->data);
}

static void _close(Enesim_Renderer_Command_State *state, Eina_Bool close)
{
	state->polygon_close(close, state->data);
}

static void _path_generate_vertices(Eina_List *commands,
		Enesim_Curve_Vertex_Add vertex_add,
		Enesim_Renderer_Path_Polygon_Add polygon_add,
		Enesim_Renderer_Path_Polygon_Close polygon_close,
		Enesim_Renderer_Path_Done path_done,
		double scale_x, double scale_y,
		const Enesim_Matrix *gm,
		void *data)
{
	Eina_List *l;
	Enesim_Renderer_Command_State state;
	Enesim_Renderer_Path_Command *cmd;

	state.vertex_add = vertex_add;
	state.polygon_add = polygon_add;
	state.polygon_close = polygon_close;
	state.path_done = path_done;
	state.data = data;

	state.st.vertex_add = vertex_add;
	state.st.last_x = 0;
	state.st.last_y = 0;
	state.st.last_ctrl_x = 0;
	state.st.last_ctrl_y = 0;
	state.st.data = data;

	EINA_LIST_FOREACH(commands, l, cmd)
	{
		double x, y;
		double rx;
		double ry;
		double ctrl_x0;
		double ctrl_y0;
		double ctrl_x1;
		double ctrl_y1;
		double ca, sa;
		/* send the new vertex to the figure renderer */
		switch (cmd->type)
		{
			case ENESIM_COMMAND_MOVE_TO:
			x = scale_x * cmd->definition.move_to.x;
			y = scale_y * cmd->definition.move_to.y;

			enesim_matrix_point_transform(gm, x, y, &x, &y);
			x = ((int) (2*x + 0.5)) / 2.0;
			y = ((int) (2*y + 0.5)) / 2.0;
			_move_to(&state, x, y);
			break;

			case ENESIM_COMMAND_LINE_TO:
			x = scale_x * cmd->definition.line_to.x;
			y = scale_y * cmd->definition.line_to.y;

			enesim_matrix_point_transform(gm, x, y, &x, &y);
			x = ((int) (2*x + 0.5)) / 2.0;
			y = ((int) (2*y + 0.5)) / 2.0;
			enesim_curve_line_to(&state.st, x, y);
			break;

			case ENESIM_COMMAND_QUADRATIC_TO:
			x = scale_x * cmd->definition.quadratic_to.x;
			y = scale_y * cmd->definition.quadratic_to.y;
			ctrl_x0 = scale_x * cmd->definition.quadratic_to.ctrl_x;
			ctrl_y0 = scale_y * cmd->definition.quadratic_to.ctrl_y;

			enesim_matrix_point_transform(gm, x, y, &x, &y);
			enesim_matrix_point_transform(gm, ctrl_x0, ctrl_y0, &ctrl_x0, &ctrl_y0);
			x = ((int) (2*x + 0.5)) / 2.0;
			y = ((int) (2*y + 0.5)) / 2.0;
			enesim_curve_quadratic_to(&state.st, ctrl_x0, ctrl_y0, x, y);
			break;

			case ENESIM_COMMAND_SQUADRATIC_TO:
			x = scale_x * cmd->definition.squadratic_to.x;
			y = scale_y * cmd->definition.squadratic_to.y;

			enesim_matrix_point_transform(gm, x, y, &x, &y);
			x = ((int) (2*x + 0.5)) / 2.0;
			y = ((int) (2*y + 0.5)) / 2.0;
			enesim_curve_squadratic_to(&state.st, x, y);
			break;

			case ENESIM_COMMAND_CUBIC_TO:
			x = scale_x * cmd->definition.cubic_to.x;
			y = scale_y * cmd->definition.cubic_to.y;
			ctrl_x0 = scale_x * cmd->definition.cubic_to.ctrl_x0;
			ctrl_y0 = scale_y * cmd->definition.cubic_to.ctrl_y0;
			ctrl_x1 = scale_x * cmd->definition.cubic_to.ctrl_x1;
			ctrl_y1 = scale_y * cmd->definition.cubic_to.ctrl_y1;

			enesim_matrix_point_transform(gm, x, y, &x, &y);
			enesim_matrix_point_transform(gm, ctrl_x0, ctrl_y0, &ctrl_x0, &ctrl_y0);
			enesim_matrix_point_transform(gm, ctrl_x1, ctrl_y1, &ctrl_x1, &ctrl_y1);
			x = ((int) (2*x + 0.5)) / 2.0;
			y = ((int) (2*y + 0.5)) / 2.0;
			enesim_curve_cubic_to(&state.st,
					ctrl_x0, ctrl_y0, ctrl_x1, ctrl_y1,
					x, y);
			break;

			case ENESIM_COMMAND_SCUBIC_TO:
			x = scale_x * cmd->definition.scubic_to.x;
			y = scale_y * cmd->definition.scubic_to.y;
			ctrl_x0 = scale_x * cmd->definition.scubic_to.ctrl_x;
			ctrl_y0 = scale_y * cmd->definition.scubic_to.ctrl_y;

			enesim_matrix_point_transform(gm, x, y, &x, &y);
			enesim_matrix_point_transform(gm, ctrl_x0, ctrl_y0, &ctrl_x0, &ctrl_y0);
			x = ((int) (2*x + 0.5)) / 2.0;
			y = ((int) (2*y + 0.5)) / 2.0;
			enesim_curve_scubic_to(&state.st, ctrl_x0, ctrl_y0,
					x, y);
			break;

			case ENESIM_COMMAND_ARC_TO:
			x = scale_x * cmd->definition.arc_to.x;
			y = scale_y * cmd->definition.arc_to.y;
			rx = scale_x * cmd->definition.arc_to.rx;
			ry = scale_y * cmd->definition.arc_to.ry;
			ca = cos(cmd->definition.arc_to.angle * M_PI / 180.0);
			sa = sin(cmd->definition.arc_to.angle * M_PI / 180.0);

			enesim_matrix_point_transform(gm, x, y, &x, &y);
			rx = rx * hypot((ca * gm->xx) + (sa * gm->xy), (ca * gm->yx) + (sa * gm->yy));
			ry = ry * hypot((ca * gm->xy) - (sa * gm->xx), (ca * gm->yy) - (sa * gm->yx));
			ca = atan2((ca * gm->yx) + (sa * gm->yy), (ca * gm->xx) + (sa * gm->xy));
			
			x = ((int) (2*x + 0.5)) / 2.0;
			y = ((int) (2*y + 0.5)) / 2.0;
			enesim_curve_arc_to(&state.st,
					rx, ry,
					ca * 180.0 / M_PI,
					cmd->definition.arc_to.large,
					cmd->definition.arc_to.sweep,
					x, y);
			break;

			case ENESIM_COMMAND_CLOSE:
			_close(&state, cmd->definition.close.close);
			break;

			default:
			break;
		}
	}
	/* in case we delay the creation of the vertices this triggers that */
	if (state.path_done)
		state.path_done(state.data);
}

static void _path_span(Enesim_Renderer *r,
		const Enesim_Renderer_State *state,
		const Enesim_Renderer_Shape_State *sstate,
		int x, int y, unsigned int len, void *ddata)
{
	Enesim_Renderer_Path *thiz;

	thiz = _path_get(r);
	enesim_renderer_sw_draw(thiz->bifigure, x, y, len, ddata);
}

static Eina_Bool _path_needs_generate(Enesim_Renderer_Path *thiz,
		const Enesim_Matrix *cgm,
		const Enesim_Matrix *pgm)
{
	Eina_Bool ret = EINA_FALSE;
	/* some command has been added */
	if (thiz->changed)
		ret = EINA_TRUE;
	/* the geometry transformation is different */
	else if (!enesim_matrix_is_equal(cgm, pgm))
		ret = EINA_TRUE;
	return ret;
}

static void _path_generate_figures(Enesim_Renderer_Path *thiz,
		Enesim_Shape_Draw_Mode dm,
		double sw,
		const Enesim_Matrix *geometry_transformation,
		double sx,
		double sy,
		Enesim_Shape_Stroke_Join join,
		Enesim_Shape_Stroke_Cap cap)
{
	if (thiz->fill_figure)
		enesim_figure_clear(thiz->fill_figure);
	else
		thiz->fill_figure = enesim_figure_new();

	if (thiz->stroke_figure)
		enesim_figure_clear(thiz->stroke_figure);
	else
		thiz->stroke_figure = enesim_figure_new();
	if ((dm & ENESIM_SHAPE_DRAW_MODE_STROKE) && (sw > 1.0))
	{
		Enesim_Renderer_Path_Stroke_State st;

		st.fill_figure = thiz->fill_figure;
		st.stroke_figure = thiz->stroke_figure;
		st.join = join;
		st.cap = cap;
		st.count = 0;
		st.rx = sw / 2 * hypot(geometry_transformation->xx, geometry_transformation->yx);
		st.ry = sw / 2 * hypot(geometry_transformation->xy, geometry_transformation->yy);

		_path_generate_vertices(thiz->commands, _stroke_path_vertex_add,
				_stroke_path_polygon_add,
				_stroke_path_polygon_close,
				_stroke_path_done,
				sx, sy,
				geometry_transformation,
				&st);
		enesim_rasterizer_figure_set(thiz->bifigure, thiz->fill_figure);
		enesim_rasterizer_bifigure_over_figure_set(thiz->bifigure, thiz->stroke_figure);
	}
	else
	{
		Enesim_Renderer_Path_Strokeless_State st;

		st.fill_figure = thiz->fill_figure;

		_path_generate_vertices(thiz->commands, _strokeless_path_vertex_add,
				_strokeless_path_polygon_add,
				_strokeless_path_polygon_close,
				NULL,
				sx, sy,
				geometry_transformation,
				&st);
		/* set the fill figure on the bifigure as its under polys */
		enesim_rasterizer_figure_set(thiz->bifigure, thiz->fill_figure);
	}
}
/*----------------------------------------------------------------------------*
 *                      The Enesim's renderer interface                       *
 *----------------------------------------------------------------------------*/
static const char * _path_name(Enesim_Renderer *r)
{
	return "path";
}

static Eina_Bool _path_sw_setup(Enesim_Renderer *r,
		const Enesim_Renderer_State *states[ENESIM_RENDERER_STATES],
		const Enesim_Renderer_Shape_State *sstates[ENESIM_RENDERER_STATES],
		Enesim_Surface *s,
		Enesim_Renderer_Shape_Sw_Draw *draw, Enesim_Error **error)
{
	Enesim_Renderer_Path *thiz;
	const Enesim_Renderer_State *cs = states[ENESIM_STATE_CURRENT];
	const Enesim_Renderer_State *ps = states[ENESIM_STATE_PAST];
	const Enesim_Renderer_Shape_State *css = sstates[ENESIM_STATE_CURRENT];

	thiz = _path_get(r);

	/* generate the list of points/polygons */
	if (_path_needs_generate(thiz, &cs->geometry_transformation, &ps->geometry_transformation))
	{
		_path_generate_figures(thiz, css->draw_mode, css->stroke.weight,
				&cs->geometry_transformation, cs->sx, cs->sy,
				css->stroke.join, css->stroke.cap);
		thiz->changed = EINA_FALSE;
	}

	enesim_renderer_shape_draw_mode_set(thiz->bifigure, css->draw_mode);
	enesim_renderer_shape_stroke_weight_set(thiz->bifigure, css->stroke.weight);
	enesim_renderer_shape_stroke_color_set(thiz->bifigure, css->stroke.color);
	enesim_renderer_shape_stroke_renderer_set(thiz->bifigure, css->stroke.r);
	enesim_renderer_shape_fill_color_set(thiz->bifigure, css->fill.color);
	enesim_renderer_shape_fill_renderer_set(thiz->bifigure, css->fill.r);
	enesim_renderer_shape_fill_rule_set(thiz->bifigure, css->fill.rule);

	enesim_renderer_color_set(thiz->bifigure, cs->color);
	enesim_renderer_origin_set(thiz->bifigure, cs->ox, cs->oy);
	enesim_renderer_transformation_set(thiz->bifigure, &cs->transformation);

	if (!enesim_renderer_setup(thiz->bifigure, s, error))
	{
		return EINA_FALSE;
	}

	*draw = _path_span;

	return EINA_TRUE;
}

static void _path_sw_cleanup(Enesim_Renderer *r, Enesim_Surface *s)
{
	Enesim_Renderer_Path *thiz;

	thiz = _path_get(r);
	enesim_renderer_shape_cleanup(r, s);
	enesim_renderer_cleanup(thiz->bifigure, s);
	thiz->changed = EINA_FALSE;
}

static void _path_flags(Enesim_Renderer *r, const Enesim_Renderer_State *state,
		Enesim_Renderer_Flag *flags)
{
	*flags = ENESIM_RENDERER_FLAG_TRANSLATE |
			ENESIM_RENDERER_FLAG_SCALE |
			ENESIM_RENDERER_FLAG_AFFINE |
			ENESIM_RENDERER_FLAG_ARGB8888 |
			ENESIM_RENDERER_FLAG_GEOMETRY; 
}

static void _path_hints(Enesim_Renderer *r, const Enesim_Renderer_State *state,
		Enesim_Renderer_Hint *hints)
{
	*hints = ENESIM_RENDERER_HINT_COLORIZE;
}

static Eina_Bool _path_has_changed(Enesim_Renderer *r,
		const Enesim_Renderer_State *states[ENESIM_RENDERER_STATES])
{
	Enesim_Renderer_Path *thiz;

	thiz = _path_get(r);
	return thiz->changed;
}

static void _path_feature_get(Enesim_Renderer *r, Enesim_Shape_Feature *features)
{
	*features = ENESIM_SHAPE_FLAG_FILL_RENDERER | ENESIM_SHAPE_FLAG_STROKE_RENDERER;
}

static void _path_boundings(Enesim_Renderer *r,
		const Enesim_Renderer_State *states[ENESIM_RENDERER_STATES],
		const Enesim_Renderer_Shape_State *sstates[ENESIM_RENDERER_STATES],
		Enesim_Rectangle *boundings)
{
	Enesim_Renderer_Path *thiz;
	const Enesim_Renderer_State *cs = states[ENESIM_STATE_CURRENT];
	const Enesim_Renderer_State *ps = states[ENESIM_STATE_PAST];
	const Enesim_Renderer_Shape_State *css = sstates[ENESIM_STATE_CURRENT];
	double xmin;
	double ymin;
	double xmax;
	double ymax;

	thiz = _path_get(r);
	if (_path_needs_generate(thiz, &cs->geometry_transformation, &ps->geometry_transformation))
	{
		_path_generate_figures(thiz, css->draw_mode, css->stroke.weight,
				&cs->geometry_transformation, cs->sx, cs->sy,
				css->stroke.join, css->stroke.cap);
		thiz->changed = EINA_FALSE;
	}

	if (!thiz->fill_figure)
	{
		boundings->x = 0;
		boundings->y = 0;
		boundings->w = 0;
		boundings->h = 0;
		return;
	}

	if ((css->draw_mode & ENESIM_SHAPE_DRAW_MODE_STROKE) && (css->stroke.weight > 1.0))
	{
		enesim_figure_boundings(thiz->stroke_figure, &xmin, &ymin, &xmax, &ymax);
	}
	else
	{
		enesim_figure_boundings(thiz->fill_figure, &xmin, &ymin, &xmax, &ymax);
	}

	boundings->x = xmin;
	boundings->w = xmax - xmin;
	boundings->y = ymin;
	boundings->h = ymax - ymin;

	/* translate by the origin */
	boundings->x += cs->ox;
	boundings->y += cs->oy;
}

static void _path_destination_boundings(Enesim_Renderer *r,
		const Enesim_Renderer_State *states[ENESIM_RENDERER_STATES],
		const Enesim_Renderer_Shape_State *sstates[ENESIM_RENDERER_STATES],
		Eina_Rectangle *boundings)
{
	Enesim_Renderer_Path *thiz;
	Enesim_Rectangle oboundings;
	const Enesim_Renderer_State *cs = states[ENESIM_STATE_CURRENT];

	thiz = _path_get(r);

	_path_boundings(r, states, sstates, &oboundings);
	if (oboundings.w == 0 && oboundings.h == 0)
	{
		boundings->x = 0;
		boundings->y = 0;
		boundings->w = 0;
		boundings->h = 0;

		return;
	}

	/* apply the inverse matrix */
	if (cs->transformation_type != ENESIM_MATRIX_IDENTITY
			&& boundings->w != INT_MAX && boundings->h != INT_MAX)
	{
		Enesim_Quad q;
		Enesim_Matrix m;

		enesim_matrix_inverse(&cs->transformation, &m);
		enesim_matrix_rectangle_transform(&m, &oboundings, &q);
		enesim_quad_rectangle_to(&q, &oboundings);
		/* fix the antialias scaling */
		oboundings.x -= m.xx;
		oboundings.y -= m.yy;
		oboundings.w += m.xx;
		oboundings.h += m.yy;
	}
	boundings->x = floor(oboundings.x);
	boundings->y = floor(oboundings.y);
	boundings->w = ceil(oboundings.x - boundings->x + oboundings.w) + 1;
	boundings->h = ceil(oboundings.y - boundings->y + oboundings.h) + 1;
}

#if BUILD_OPENGL
static Eina_Bool _path_opengl_setup(Enesim_Renderer *r,
		const Enesim_Renderer_State *states[ENESIM_RENDERER_STATES],
		const Enesim_Renderer_Shape_State *sstates[ENESIM_RENDERER_STATES],
		Enesim_Surface *s,
		int *num_shaders,
		Enesim_Renderer_OpenGL_Shader **shaders,
		Enesim_Error **error)
{
	Enesim_Renderer_Path *thiz;
 	thiz = _path_get(r);

	/*
	 * On the gl version use the tesselator to generate the triangles and pass them
	 * to the goemetry shader. Only tesselate again whenever something has changed
	 */
#if 0
	tobj = gluNewTess();
	gluTessCallback(tobj, GLU_TESS_VERTEX,
			   (GLvoid (*) ()) &glVertex3dv);
	gluTessCallback(tobj, GLU_TESS_BEGIN,
			   (GLvoid (*) ()) &beginCallback);
	gluTessCallback(tobj, GLU_TESS_END,
			   (GLvoid (*) ()) &endCallback);
	gluTessCallback(tobj, GLU_TESS_ERROR,
			   (GLvoid (*) ()) &errorCallback);

	/*  new callback routines registered by these calls */
	void vertexCallback(GLvoid *vertex)
	{
		const GLdouble *pointer;

		pointer = (GLdouble *) vertex;
		glColor3dv(pointer+3);
		glVertex3dv(vertex);
	}

	void combineCallback(GLdouble coords[3], 
			     GLdouble *vertex_data[4],
			     GLfloat weight[4], GLdouble **dataOut )
	{
		GLdouble *vertex;
		int i;

		vertex = (GLdouble *) malloc(6 * sizeof(GLdouble));
		vertex[0] = coords[0];
		vertex[1] = coords[1];
		vertex[2] = coords[2];
		for (i = 3; i < 7; i++)
		vertex[i] = weight[0] * vertex_data[0][i] 
			  + weight[1] * vertex_data[1][i]
			  + weight[2] * vertex_data[2][i] 
			  + weight[3] * vertex_data[3][i];
		*dataOut = vertex;
	}

	/*  the callback routines registered by gluTessCallback() */

	void beginCallback(GLenum which)
	{
	}

	void endCallback(void)
	{
	}
	// describe non-convex polygon
	gluTessBeginPolygon(tess, user_data);
		// first contour
		gluTessBeginContour(tess);
		gluTessVertex(tess, coords[0], vertex_data);
		...
		gluTessEndContour(tess);

		// second contour
		gluTessBeginContour(tess);
		gluTessVertex(tess, coords[5], vertex_data);
		...
		gluTessEndContour(tess);
		...
	gluTessEndPolygon(tess);

#endif
	/* after tesselating we should have a structure with our triangles that we should
	 * pass to the geometry shader through uniforms
	 * the shader should only emit primitives and vertices
	 */
	return EINA_TRUE;
}

static Eina_Bool _path_opengl_shader_setup(Enesim_Renderer *r, Enesim_Surface *s)
{
	Enesim_Renderer_Path *thiz;
	Enesim_Renderer_OpenGL_Data *rdata;
	int final_color;

 	thiz = _path_get(r);

	return EINA_TRUE;
}

static void _path_opengl_cleanup(Enesim_Renderer *r, Enesim_Surface *s)
{
	Enesim_Renderer_Path *thiz;

 	thiz = _path_get(r);
}
#endif

static Enesim_Renderer_Shape_Descriptor _path_descriptor = {
	/* .name = 			*/ _path_name,
	/* .free = 			*/ NULL,
	/* .boundings = 		*/ _path_boundings,
	/* .destination_boundings = 	*/ _path_destination_boundings,
	/* .flags = 			*/ _path_flags,
	/* .hints_get = 		*/ _path_hints,
	/* .is_inside = 		*/ NULL,
	/* .damage = 			*/ NULL,
	/* .has_changed = 		*/ _path_has_changed,
	/* .feature_get =		*/ _path_feature_get,
	/* .sw_setup = 			*/ _path_sw_setup,
	/* .sw_cleanup = 		*/ _path_sw_cleanup,
	/* .opencl_setup =		*/ NULL,
	/* .opencl_kernel_setup =	*/ NULL,
	/* .opencl_cleanup =		*/ NULL,
#if BUILD_OPENGL
	/* .opengl_setup = 		*/ _path_opengl_setup,
	/* .opengl_shader_setup =	*/ _path_opengl_shader_setup,
	/* .opengl_cleanup =		*/ _path_opengl_cleanup
#else
	/* .opengl_setup = 		*/ NULL,
	/* .opengl_shader_setup =	*/ NULL,
	/* .opengl_cleanup = 		*/ NULL
#endif
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
EAPI Enesim_Renderer * enesim_renderer_path_new(void)
{
	Enesim_Renderer *r;
	Enesim_Renderer_Path *thiz;

	thiz = calloc(1, sizeof(Enesim_Renderer_Path));
	if (!thiz) return NULL;
	EINA_MAGIC_SET(thiz, ENESIM_RENDERER_PATH_MAGIC);

	r = enesim_rasterizer_bifigure_new();
	if (!r) goto err_figure;
	thiz->bifigure = r;

	r = enesim_renderer_shape_new(&_path_descriptor, thiz);
	/* FIXME for now */
	enesim_renderer_shape_stroke_join_set(r, ENESIM_JOIN_ROUND);
	return r;

err_figure:
	free(thiz);
	return NULL;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_path_command_clear(Enesim_Renderer *r)
{
	Enesim_Renderer_Path *thiz;
	Enesim_Renderer_Path_Command *c;

	thiz = _path_get(r);
	EINA_LIST_FREE(thiz->commands, c)
	{
		free(c);
	}
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_path_command_add(Enesim_Renderer *r, Enesim_Renderer_Path_Command *cmd)
{
	Enesim_Renderer_Path *thiz;
	Enesim_Renderer_Path_Command *new_command;

	thiz = _path_get(r);

	new_command = malloc(sizeof(Enesim_Renderer_Path_Command));
	*new_command = *cmd;
	thiz->commands = eina_list_append(thiz->commands, new_command);
	thiz->changed = EINA_TRUE;
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_path_command_set(Enesim_Renderer *r,
		Eina_List *list)
{
	Enesim_Renderer_Path_Command *cmd;
	Eina_List *l;

	enesim_renderer_path_command_clear(r);
	EINA_LIST_FOREACH(list, l, cmd)
	{
		enesim_renderer_path_command_add(r, cmd);
	}
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_path_move_to(Enesim_Renderer *r, double x, double y)
{
	Enesim_Renderer_Path_Command cmd;

	cmd.type = ENESIM_COMMAND_MOVE_TO;
	cmd.definition.move_to.x = x;
	cmd.definition.move_to.y = y;
	enesim_renderer_path_command_add(r, &cmd);
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_path_line_to(Enesim_Renderer *r, double x, double y)
{
	Enesim_Renderer_Path_Command cmd;

	cmd.type = ENESIM_COMMAND_LINE_TO;
	cmd.definition.line_to.x = x;
	cmd.definition.line_to.y = y;
	enesim_renderer_path_command_add(r, &cmd);
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_path_squadratic_to(Enesim_Renderer *r, double x,
		double y)
{
	Enesim_Renderer_Path_Command cmd;

	cmd.type = ENESIM_COMMAND_SQUADRATIC_TO;
	cmd.definition.squadratic_to.x = x;
	cmd.definition.squadratic_to.y = y;
	enesim_renderer_path_command_add(r, &cmd);
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_path_quadratic_to(Enesim_Renderer *r, double ctrl_x,
		double ctrl_y, double x, double y)
{
	Enesim_Renderer_Path_Command cmd;

	cmd.type = ENESIM_COMMAND_QUADRATIC_TO;
	cmd.definition.quadratic_to.x = x;
	cmd.definition.quadratic_to.y = y;
	cmd.definition.quadratic_to.ctrl_x = ctrl_x;
	cmd.definition.quadratic_to.ctrl_y = ctrl_y;
	enesim_renderer_path_command_add(r, &cmd);
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_path_cubic_to(Enesim_Renderer *r, double ctrl_x0,
		double ctrl_y0, double ctrl_x, double ctrl_y, double x, double y)
{
	Enesim_Renderer_Path_Command cmd;

	cmd.type = ENESIM_COMMAND_CUBIC_TO;
	cmd.definition.cubic_to.x = x;
	cmd.definition.cubic_to.y = y;
	cmd.definition.cubic_to.ctrl_x0 = ctrl_x0;
	cmd.definition.cubic_to.ctrl_y0 = ctrl_y0;
	cmd.definition.cubic_to.ctrl_x1 = ctrl_x;
	cmd.definition.cubic_to.ctrl_y1 = ctrl_y;
	enesim_renderer_path_command_add(r, &cmd);
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_path_scubic_to(Enesim_Renderer *r, double ctrl_x,
		double ctrl_y, double x, double y)
{
	Enesim_Renderer_Path_Command cmd;

	cmd.type = ENESIM_COMMAND_SCUBIC_TO;
	cmd.definition.scubic_to.x = x;
	cmd.definition.scubic_to.y = y;
	cmd.definition.scubic_to.ctrl_x = ctrl_x;
	cmd.definition.scubic_to.ctrl_y = ctrl_y;
	enesim_renderer_path_command_add(r, &cmd);
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_path_arc_to(Enesim_Renderer *r, double rx, double ry, double angle,
                   unsigned char large, unsigned char sweep, double x, double y)
{
	Enesim_Renderer_Path_Command cmd;

	cmd.type = ENESIM_COMMAND_ARC_TO;
	cmd.definition.arc_to.x = x;
	cmd.definition.arc_to.y = y;
	cmd.definition.arc_to.rx = rx;
	cmd.definition.arc_to.ry = ry;
	cmd.definition.arc_to.angle = angle;
	cmd.definition.arc_to.large = large;
	cmd.definition.arc_to.sweep = sweep;
	enesim_renderer_path_command_add(r, &cmd);
}

/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI void enesim_renderer_path_close(Enesim_Renderer *r, Eina_Bool close)
{
	Enesim_Renderer_Path_Command cmd;

	cmd.type = ENESIM_COMMAND_CLOSE;
	cmd.definition.close.close = close;
	enesim_renderer_path_command_add(r, &cmd);
}
