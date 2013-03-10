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
#ifndef _ENESIM_PATH_NORMALIZER_PRIVATE_H
#define _ENESIM_PATH_NORMALIZER_PRIVATE_H

typedef struct _Enesim_Path_Normalizer Enesim_Path_Normalizer;

typedef void (*Enesim_Path_Normalizer_Path_Move_To_Cb)(Enesim_Path_Command_Move_To *move_to,
		void *data);
typedef void (*Enesim_Path_Normalizer_Path_Line_To_Cb)(Enesim_Path_Command_Line_To *line_to,
		void *data);
typedef void (*Enesim_Path_Normalizer_Path_Cubic_To_Cb)(Enesim_Path_Command_Cubic_To *cubic_to,
		void *data);
typedef void (*Enesim_Path_Normalizer_Path_Close_Cb)(Enesim_Path_Command_Close *close,
		void *data);

typedef struct _Enesim_Path_Normalizer_Path_Descriptor {
	Enesim_Path_Normalizer_Path_Move_To_Cb move_to;
	Enesim_Path_Normalizer_Path_Line_To_Cb line_to;
	Enesim_Path_Normalizer_Path_Cubic_To_Cb cubic_to;
	Enesim_Path_Normalizer_Path_Close_Cb close;
} Enesim_Path_Normalizer_Path_Descriptor;

Enesim_Path_Normalizer * enesim_path_normalizer_path_new(
		Enesim_Path_Normalizer_Path_Descriptor *descriptor,
		void *data);

typedef void (*Enesim_Path_Normalizer_Figure_Vertex_Add)(double x, double y, void *data);
typedef void (*Enesim_Path_Normalizer_Figure_Polygon_Add)(void *data);
typedef void (*Enesim_Path_Normalizer_Figure_Polygon_Close)(Eina_Bool close, void *data);

typedef struct _Enesim_Path_Normalizer_Figure_Descriptor {
	Enesim_Path_Normalizer_Figure_Vertex_Add vertex_add;
	Enesim_Path_Normalizer_Figure_Polygon_Add polygon_add;
	Enesim_Path_Normalizer_Figure_Polygon_Close polygon_close;
} Enesim_Path_Normalizer_Figure_Descriptor;

Enesim_Path_Normalizer * enesim_path_normalizer_figure_new(
		Enesim_Path_Normalizer_Figure_Descriptor *descriptor,
		void *data);

void enesim_path_normalizer_normalize(Enesim_Path_Normalizer *thiz,
		Enesim_Path_Command *cmd);
void enesim_path_normalizer_line_to(Enesim_Path_Normalizer *thiz,
		Enesim_Path_Command_Line_To *line_to);
void enesim_path_normalizer_squadratic_to(Enesim_Path_Normalizer *thiz,
		Enesim_Path_Command_Squadratic_To *squadratic);
void enesim_path_normalizer_quadratic_to(Enesim_Path_Normalizer *thiz,
		Enesim_Path_Command_Quadratic_To *quadratic);
void enesim_path_normalizer_cubic_to(Enesim_Path_Normalizer *thiz,
		Enesim_Path_Command_Cubic_To *cubic_to);
void enesim_path_normalizer_move_to(Enesim_Path_Normalizer *thiz,
		Enesim_Path_Command_Move_To *move_to);
void enesim_path_normalizer_arc_to(Enesim_Path_Normalizer *thiz,
		Enesim_Path_Command_Arc_To *arc);
void enesim_path_normalizer_scubic_to(Enesim_Path_Normalizer *thiz,
		Enesim_Path_Command_Scubic_To *scubic_to);
void enesim_path_normalizer_close(Enesim_Path_Normalizer *thiz,
		Enesim_Path_Command_Close *close);
void enesim_path_normalizer_free(Enesim_Path_Normalizer *thiz);
void enesim_path_normalizer_reset(Enesim_Path_Normalizer *thiz);

#endif
