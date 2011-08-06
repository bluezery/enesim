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
#ifndef MATRIX_H_
#define MATRIX_H_

#define MATRIX_XX(m) m->xx
#define MATRIX_XY(m) m->xy
#define MATRIX_XZ(m) m->xz
#define MATRIX_YX(m) m->yx
#define MATRIX_YY(m) m->yy
#define MATRIX_YZ(m) m->yz
#define MATRIX_ZX(m) m->zx
#define MATRIX_ZY(m) m->zy
#define MATRIX_ZZ(m) m->zz
#define MATRIX_SIZE 9

#define QUAD_X0(q) q->x0
#define QUAD_Y0(q) q->y0
#define QUAD_X1(q) q->x1
#define QUAD_Y1(q) q->y1
#define QUAD_X2(q) q->x2
#define QUAD_Y2(q) q->y2
#define QUAD_X3(q) q->x3
#define QUAD_Y3(q) q->y3

#endif /*MATRIX_H_*/
