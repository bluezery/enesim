#ifndef _ENESIM_TEXT_PRIVATE_H
#define _ENESIM_TEXT_PRIVATE_H

/* SIMD intrinsics */
#ifdef ENS_HAVE_MMX
#define LIBARGB_MMX 1
#else
#define LIBARGB_MMX 0
#endif

#ifdef  ENS_HAVE_SSE
#define LIBARGB_SSE 1
#else
#define LIBARGB_SSE 0
#endif

#ifdef ENS_HAVE_SSE2
#define LIBARGB_SSE2 1
#else
#define LIBARGB_SSE2 0
#endif

#define LIBARGB_DEBUG 0

#include "libargb.h"

/* freetype engine */
void enesim_text_engine_freetype_init(void);
void enesim_text_engine_freetype_shutdown(void);

/* text buffer interface */
typedef void (*Enesim_Text_Buffer_String_Set)(void *data, const char *string, int length);
typedef const char * (*Enesim_Text_Buffer_String_Get)(void *data);
typedef int (*Enesim_Text_Buffer_String_Insert)(void *data, const char *string, int length, ssize_t offset);
typedef int (*Enesim_Text_Buffer_String_Delete)(void *data, int length, ssize_t offset);
typedef int (*Enesim_Text_Buffer_String_Length)(void *data);
typedef void (*Enesim_Text_Buffer_Free)(void *data);

typedef struct _Enesim_Text_Buffer_Descriptor
{
	Enesim_Text_Buffer_String_Get string_get;
	Enesim_Text_Buffer_String_Set string_set;
	Enesim_Text_Buffer_String_Insert string_insert;
	Enesim_Text_Buffer_String_Delete string_delete;
	Enesim_Text_Buffer_String_Length string_length;
	Enesim_Text_Buffer_Free free;
} Enesim_Text_Buffer_Descriptor;

Enesim_Text_Buffer * enesim_text_buffer_new_from_descriptor(Enesim_Text_Buffer_Descriptor *descriptor, void *data);
void * enesim_text_buffer_data_get(Enesim_Text_Buffer *thiz);

/* font interface */
struct _Enesim_Text_Font
{
	Enesim_Text_Engine *engine;
	void *data;
	Eina_Hash *glyphs;
	char *key;
	int ref;
};

typedef struct _Enesim_Text_Glyph Enesim_Text_Glyph;
typedef struct _Enesim_Text_Glyph_Position Enesim_Text_Glyph_Position;

struct _Enesim_Text_Glyph
{
	Enesim_Surface *surface;
	/* temporary */
	int origin;
	int x_advance;
	/* TODO */
	int ref;
};

struct _Enesim_Text_Glyph_Position
{
	int index; /**< The index on the string */
	int distance; /**< Number of pixels from the origin to the glyph */
	Enesim_Text_Glyph *glyph; /**< The glyph at this position */
};

Enesim_Text_Glyph * enesim_text_font_glyph_get(Enesim_Text_Font *f, char c);
Enesim_Text_Glyph * enesim_text_font_glyph_load(Enesim_Text_Font *f, char c);
void enesim_text_font_glyph_unload(Enesim_Text_Font *f, char c);

void enesim_text_font_dump(Enesim_Text_Font *f, const char *path);

/* engine interface */
typedef struct _Enesim_Text_Engine_Descriptor
{
	/* main interface */
	void *(*init)(void);
	void (*shutdown)(void *data);
	/* font interface */
	void *(*font_load)(void *data, const char *name, int index, int size);
	void (*font_delete)(void *data, void *fdata);
	int (*font_max_ascent_get)(void *data, void *fdata);
	int (*font_max_descent_get)(void *data, void *fdata);
	void (*font_glyph_get)(void *data, void *fdata, char c, Enesim_Text_Glyph *g);
} Enesim_Text_Engine_Descriptor;

Enesim_Text_Engine * enesim_text_engine_new(Enesim_Text_Engine_Descriptor *d);
void enesim_text_engine_free(Enesim_Text_Engine *thiz);
Enesim_Text_Font * enesim_text_engine_font_new(
		Enesim_Text_Engine *thiz, const char *file, int index, int size);
void enesim_text_engine_font_delete(Enesim_Text_Engine *thiz,
		Enesim_Text_Font *f);

/* main interface */
struct _Enesim_Text_Engine
{
	Enesim_Text_Engine_Descriptor *d;
	Eina_Hash *fonts;
	int ref;
	void *data;
};

void enesim_text_init(void);
void enesim_text_shutdown(void);

#endif
