/* ENESIM - Drawing Library
 * Copyright (C) 2007-2013 Jorge Luis Zapata
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
#ifndef CONVERTER_H_
#define CONVERTER_H_

typedef void (*Enesim_Converter_2D)(Enesim_Buffer_Sw_Data *ddata, uint32_t dw, uint32_t dh,
		Enesim_Buffer_Sw_Data *sdata, uint32_t sw, uint32_t sh);

#define ENESIM_CONVERTER_2D(f) ((Enesim_Converter_2D)(f))

void enesim_converter_init(void);
void enesim_converter_shutdown(void);

void enesim_converter_argb8888_init(void);
void enesim_converter_rgb888_init(void);
void enesim_converter_bgr888_init(void);
void enesim_converter_rgb565_init(void);
void enesim_converter_a8_init(void);

void enesim_converter_surface_register(Enesim_Converter_2D cnv,
		Enesim_Buffer_Format dfmt, Enesim_Angle angle, Enesim_Buffer_Format sfmt);

Enesim_Converter_2D enesim_converter_surface_get(Enesim_Buffer_Format dfmt,
		Enesim_Angle angle, Enesim_Buffer_Format sfmt);

#endif
