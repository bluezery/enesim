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
#ifndef ENESIM_BUFFER_H_
#define ENESIM_BUFFER_H_

/**
 * @defgroup Enesim_Buffer_Group Buffer
 * @brief Target device pixel data holder
 * @{
 */

typedef struct _Enesim_Buffer Enesim_Buffer; /**< Buffer Handler */

/*
 * ENESIM_BUFFER_FORMAT_A8
 * +---------------+----------------+
 * |     Alpha     |      Alpha     |
 * +---------------+----------------+
 *         8                8
 * <------P0------>.<------P1------>.
 *
 * ENESIM_BUFFER_FORMAT_b1A3
 * +-------+-------+--------+-------+
 * | Blink | Alpha |  Blink | Alpha |
 * +-------+-------+--------+-------+
 *     1       3        1        3
 * <------P0------>.<------P1------>.
 */

/**
 * Enumeration of the different buffer formats
 */ 
typedef enum _Enesim_Buffer_Format
{
	ENESIM_BUFFER_FORMAT_RGB565, /**< 16bpp RGB 565 */
	ENESIM_BUFFER_FORMAT_ARGB8888, /**< 32bpp ARGB 8888 */
	ENESIM_BUFFER_FORMAT_ARGB8888_PRE, /**< 32bpp ARGB premultiplied 8888*/
	ENESIM_BUFFER_FORMAT_XRGB8888, /**< 32bpp RGB 888 */
	ENESIM_BUFFER_FORMAT_RGB888, /**< 24bpp RGB 888 */
	ENESIM_BUFFER_FORMAT_BGR888, /**< 24bpp BGR 888 */
	ENESIM_BUFFER_FORMAT_A8, /**< 8bpp A 8 */
	ENESIM_BUFFER_FORMAT_GRAY, /**< 8bpp Grayscale */
	ENESIM_BUFFER_FORMAT_CMYK,
	ENESIM_BUFFER_FORMAT_CMYK_ADOBE,
	ENESIM_BUFFER_FORMATS
} Enesim_Buffer_Format;

/**
 * Definition of a 24 bits per pixel format
 */
typedef struct _Enesim_Buffer_24bpp
{
	uint8_t *plane0;
	int plane0_stride;
} Enesim_Buffer_24bpp;

/**
 * Definition of a 32 bits per pixel format
 */
typedef struct _Enesim_Buffer_32bpp
{
	uint32_t *plane0;
	int plane0_stride;
} Enesim_Buffer_32bpp;

typedef struct _Enesim_Buffer_Rgb565
{
	uint16_t *plane0;
	int plane0_stride;
} Enesim_Buffer_Rgb565;

/**
 * Definition of a 8 bits alpha only format
 */
typedef struct _Enesim_Buffer_A8
{
	uint8_t *plane0;
	int plane0_stride;
} Enesim_Buffer_A8;

typedef Enesim_Buffer_32bpp Enesim_Buffer_Argb8888;
typedef Enesim_Buffer_32bpp Enesim_Buffer_Argb8888_Pre;
typedef Enesim_Buffer_32bpp Enesim_Buffer_Xrgb8888;

typedef Enesim_Buffer_24bpp Enesim_Buffer_Rgb888;
typedef Enesim_Buffer_24bpp Enesim_Buffer_Bgr888;
typedef Enesim_Buffer_24bpp Enesim_Buffer_Cmyk;

typedef union _Enesim_Buffer_Sw_Data
{
	Enesim_Buffer_Argb8888 argb8888;
	Enesim_Buffer_Argb8888_Pre argb8888_pre;
	Enesim_Buffer_Xrgb8888 xrgb8888;
	Enesim_Buffer_Rgb565 rgb565;
	Enesim_Buffer_A8 a8;
	Enesim_Buffer_Rgb888 rgb888;
	Enesim_Buffer_Bgr888 bgr888;
	Enesim_Buffer_Cmyk cmyk;
} Enesim_Buffer_Sw_Data;

typedef void (*Enesim_Buffer_Free)(void *data, void *user_data);

EAPI Enesim_Buffer * enesim_buffer_new(Enesim_Buffer_Format f, uint32_t w, uint32_t h);
EAPI Enesim_Buffer * enesim_buffer_new_data_from(Enesim_Buffer_Format f,
		uint32_t w, uint32_t h, Eina_Bool copy,
		Enesim_Buffer_Sw_Data *data, Enesim_Buffer_Free free_func,
		void *free_func_data);
EAPI Enesim_Buffer * enesim_buffer_new_pool_from(Enesim_Buffer_Format f, uint32_t w, uint32_t h, Enesim_Pool *p);
EAPI Enesim_Buffer * enesim_buffer_new_pool_and_data_from(Enesim_Buffer_Format f,
		uint32_t w, uint32_t h, Enesim_Pool *p, Eina_Bool copy,
		Enesim_Buffer_Sw_Data *data, Enesim_Buffer_Free free_func,
		void *free_func_data);

EAPI Enesim_Buffer * enesim_buffer_ref(Enesim_Buffer *b);
EAPI void enesim_buffer_unref(Enesim_Buffer *b);

EAPI void enesim_buffer_size_get(const Enesim_Buffer *b, int *w, int *h);
EAPI Enesim_Buffer_Format enesim_buffer_format_get(const Enesim_Buffer *b);
EAPI Enesim_Backend enesim_buffer_backend_get(const Enesim_Buffer *b);
EAPI Enesim_Pool * enesim_buffer_pool_get(Enesim_Buffer *b);

EAPI void enesim_buffer_private_set(Enesim_Buffer *b, void *data);
EAPI void * enesim_buffer_private_get(Enesim_Buffer *b);

EAPI Eina_Bool enesim_buffer_data_get(const Enesim_Buffer *b, Enesim_Buffer_Sw_Data *data);

EAPI size_t enesim_buffer_format_size_get(Enesim_Buffer_Format fmt, uint32_t w, uint32_t h);

EAPI Eina_Bool enesim_buffer_format_rgb_components_from(Enesim_Buffer_Format *fmt,
		uint8_t aoffset, uint8_t alen,
		uint8_t roffset, uint8_t rlen,
		uint8_t goffset, uint8_t glen,
		uint8_t boffset, uint8_t blen, Eina_Bool premul);
EAPI Eina_Bool enesim_buffer_format_rgb_components_to(Enesim_Buffer_Format fmt,
		uint8_t *aoffset, uint8_t *alen,
		uint8_t *roffset, uint8_t *rlen,
		uint8_t *goffset, uint8_t *glen,
		uint8_t *boffset, uint8_t *blen, Eina_Bool *premul);
EAPI uint8_t enesim_buffer_format_rgb_depth_get(Enesim_Buffer_Format fmt);

EAPI void enesim_buffer_lock(Enesim_Buffer *b, Eina_Bool write);
EAPI void enesim_buffer_unlock(Enesim_Buffer *b);

/** @} */ //End of Enesim_Buffer_Group


#endif /*ENESIM_BUFFER_H_*/
