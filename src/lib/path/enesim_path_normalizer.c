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

#include "enesim_path.h"
#include "enesim_path_normalizer_private.h"

typedef void (*Enesim_Path_Normalizer_Move_To_Cb)(Enesim_Path_Command_Move_To move_to,
		Enesim_Path_Normalizer_State *state, void *data);
typedef void (*Enesim_Path_Normalizer_Line_To_Cb)(Enesim_Path_Command_Line_To line_to,
		Enesim_Path_Normalizer_State *state, void *data);
typedef void (*Enesim_Path_Normalizer_Cubic_To_Cb)(Enesim_Path_Command_Cubic_To cubic_to,
		Enesim_Path_Normalizer_State *state, void *data);
typedef void (*Enesim_Path_Normalizer_Close_Cb)(Enesim_Path_Command_Close close,
		Enesim_Path_Normalizer_State *state, void *data);

typedef struct _Enesim_Path_Normalizer_State
{
	double last_x;
	double last_y;
	double last_ctrl_x;
	double last_ctrl_y;
} Enesim_Path_Normalizer_State;

typedef struct _Enesim_Path_Normalizer_Descriptor
{
	Enesim_Path_Normalizer_Move_To_Cb move_to;
	Enesim_Path_Normalizer_Line_To_Cb line_to;
	Enesim_Path_Normalizer_Cubic_To_Cb cubic_to;
} Enesim_Path_Normalizer_Descriptor;

typedef struct _Enesim_Path_Normalizer
{
	Enesim_Path_Normalizer_Descriptor *descriptor;
	Enesim_Path_Normalizer_State state;
	void *data;
} Enesim_Path_Normalizer;
/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/
/*----------------------------------------------------------------------------*
 *                            Figure normalizer                               *
 *----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*
 *                             Path normalizer                                *
 *----------------------------------------------------------------------------*/
/*============================================================================*
 *                                 Global                                     *
 *============================================================================*/
Enesim_Path_Normalizer * enesim_path_normalizer_new(
		Enesim_Path_Normalizer_Descriptor *descriptor,
		void *data)
{
	Enesim_Path_Normalizer *thiz;

	thiz = calloc(1, sizeof(Enesim_Path_Normalizer));
	thiz->data = data;
	thiz->descriptor = descriptor;
	return thiz;
}

/* generate a figure only (line, move, close) commands */
Enesim_Path_Normalizer * enesim_path_normalizer_figure_new(
		Enesim_Path_Normalizer_Figure_Descriptor *descriptor,
		void *data)
{

}

/* generate a path simplifed (line, move, cubic, close) */
Enesim_Path_Normalizer * enesim_path_normalizer_path_new(
		Enesim_Path_Normalizer_Path *descriptor,
		void *data)
{

}

void enesim_path_normalizer_normalize(Enesim_Path_Normalizer *thiz,
		Enesim_Path_Command *cmd)
{
	switch (cmd->type)
	{
		case ENESIM_PATH_COMMAND_MOVE_TO:
		break;

		case ENESIM_PATH_COMMAND_LINE_TO:
		break;

		case ENESIM_PATH_COMMAND_QUADRATIC_TO:
		break;

		case ENESIM_PATH_COMMAND_SQUADRATIC_TO:
		break;

		case ENESIM_PATH_COMMAND_CUBIC_TO:
		break;

		case ENESIM_PATH_COMMAND_SCUBIC_TO:
		break;

		case ENESIM_PATH_COMMAND_ARC_TO:
		break;

		case ENESIM_PATH_COMMAND_CLOSE:
		break;
	}
}
