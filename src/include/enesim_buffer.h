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
#ifndef ENESIM_BUFFER_H_
#define ENESIM_BUFFER_H_

/**
 * @defgroup Enesim_Buffer_Group Buffer
 * @{
 *
 * @todo
 */
typedef struct _Enesim_Buffer Enesim_Buffer; /**< Buffer Handler */

/*
 * ENESIM_CONVERTER_FORMAT_A8
 * +---------------+----------------+
 * |     Alpha     |      Alpha     |
 * +---------------+----------------+
 *         8                8
 * <------P0------>.<------P1------>.
 *
 * ENESIM_CONVERTER_FORMAT_b1A3
 * +-------+-------+--------+-------+
 * | Blink | Alpha |  Blink | Alpha |
 * +-------+-------+--------+-------+
 *     1       3        1        3
 * <------P0------>.<------P1------>.
 */

typedef enum _Enesim_Buffer_Format
{
	ENESIM_CONVERTER_RGB565,
	ENESIM_CONVERTER_ARGB8888,
	ENESIM_CONVERTER_ARGB8888_PRE,
	ENESIM_CONVERTER_RGB888,
	ENESIM_CONVERTER_BGR888,
	ENESIM_CONVERTER_A8,
	ENESIM_CONVERTER_GRAY,
	ENESIM_CONVERTER_FORMATS
} Enesim_Buffer_Format;

typedef struct _Enesim_Buffer_Argb8888
{
	uint32_t *plane0;
	int plane0_stride;
} Enesim_Buffer_Argb8888;

typedef struct _Enesim_Buffer_Rgb565
{
	uint16_t *plane0;
	int plane0_stride;
} Enesim_Buffer_Rgb565;

typedef struct _Enesim_Buffer_A8
{
	uint8_t *plane0;
	int plane0_stride;
} Enesim_Buffer_A8;

struct _Enesim_Buffer_24bpp
{
	uint8_t *plane0;
	int plane0_stride;
};

typedef struct _Enesim_Buffer_24bpp Enesim_Buffer_Rgb888;
typedef struct _Enesim_Buffer_24bpp Enesim_Buffer_Bgr888;

typedef union _Enesim_Buffer_Sw_Data
{
	Enesim_Buffer_Argb8888 argb8888;
	Enesim_Buffer_Argb8888 argb8888_pre;
	Enesim_Buffer_Rgb565 rgb565;
	Enesim_Buffer_A8 a8;
	Enesim_Buffer_Rgb888 rgb888;
	Enesim_Buffer_Bgr888 bgr888;
} Enesim_Buffer_Sw_Data;

EAPI Enesim_Buffer * enesim_buffer_new(Enesim_Buffer_Format f, uint32_t w, uint32_t h);
EAPI Enesim_Buffer * enesim_buffer_new_data_from(Enesim_Buffer_Format f, uint32_t w, uint32_t h, Eina_Bool copy, Enesim_Buffer_Sw_Data *data);
EAPI Enesim_Buffer * enesim_buffer_new_pool_from(Enesim_Buffer_Format f, uint32_t w, uint32_t h, Enesim_Pool *p);
EAPI Enesim_Buffer * enesim_buffer_new_pool_and_data_from(Enesim_Buffer_Format f,
		uint32_t w, uint32_t h, Enesim_Pool *p, Eina_Bool copy, Enesim_Buffer_Sw_Data *data);

EAPI void enesim_buffer_delete(Enesim_Buffer *b);

EAPI void enesim_buffer_size_get(const Enesim_Buffer *b, int *w, int *h);
EAPI Enesim_Buffer_Format enesim_buffer_format_get(const Enesim_Buffer *b);
EAPI Enesim_Backend enesim_buffer_backend_get(const Enesim_Buffer *b);

EAPI void enesim_buffer_private_set(Enesim_Buffer *b, void *data);
EAPI void * enesim_buffer_private_get(Enesim_Buffer *b);

EAPI Eina_Bool enesim_buffer_data_get(const Enesim_Buffer *b, Enesim_Buffer_Sw_Data *data);

EAPI size_t enesim_buffer_format_size_get(Enesim_Buffer_Format fmt, uint32_t w, uint32_t h);

/** @} */ //End of Enesim_Buffer_Group


#endif /*ENESIM_BUFFER_H_*/
