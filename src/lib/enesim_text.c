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
#include "enesim_private.h"

#include "enesim_text.h"
#include "enesim_text_private.h"
/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/
static int _enesim_text_init_count = 0;

static void _enesim_text_setup(Enesim_Text_Engine *e)
{
	e->data = e->engine->init();
	e->fonts = eina_hash_string_superfast_new(NULL);
}

/*============================================================================*
 *                                 Global                                     *
 *============================================================================*/
int enesim_text_log_dom_global = -1;

#if HAVE_FREETYPE
extern Enesim_Text_Engine enesim_text_freetype;
#endif
/*============================================================================*
 *                                   API                                      *
 *============================================================================*/
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI int enesim_text_init(void)
{
	if (++_enesim_text_init_count != 1)
		return _enesim_text_init_count;

	if (!eina_init())
	{
		fprintf(stderr, "Enesim_Text_Engine: Eina init failed");
		return --_enesim_text_init_count;
	}

	enesim_text_log_dom_global = eina_log_domain_register("etex", ENESIM_TEXT_LOG_COLOR_DEFAULT);
	if (enesim_text_log_dom_global < 0)
	{
		EINA_LOG_ERR("Enesim_Text_Engine: Can not create a general log domain.");
		goto shutdown_eina;
	}

	if (!enesim_init())
	{
		ERR("Enesim init failed");
		goto unregister_log_domain;
	}

	return _enesim_text_init_count;

  unregister_log_domain:
	eina_log_domain_unregister(enesim_text_log_dom_global);
	enesim_text_log_dom_global = -1;
  shutdown_eina:
	eina_shutdown();
	return --_enesim_text_init_count;
}
/**
 * To be documented
 * FIXME: To be fixed
 */
EAPI int enesim_text_shutdown(void)
{
	if (--_enesim_text_init_count != 0)
		return _enesim_text_init_count;

	enesim_shutdown();
	eina_log_domain_unregister(enesim_text_log_dom_global);
	enesim_text_log_dom_global = -1;
	eina_shutdown();

	return _enesim_text_init_count;
}
/**
 * To be documented
 * FIXME: To be fixed
 * TODO change this to _get()
 * add a refcount and on deletion check that
 */
EAPI Enesim_Text_Engine * enesim_text_freetype_get(void)
{
#if HAVE_FREETYPE
	static Enesim_Text_Engine *e = NULL;

	if (!e)
	{
		e = calloc(1, sizeof(Enesim_Text_Engine));
		e->engine = &enesim_text_freetype;
		_enesim_text_setup(e);
	}
	return e;
#else
	return NULL;
#endif
}

/**
 * To be documented
 * FIXME: To be fixed
 * FIXME: remove this!
 */
EAPI void enesim_text_engine_delete(Enesim_Text_Engine *e)
{
#if HAVE_FREETYPE
	e->engine->shutdown(e->data);
#endif
}

/**
 *
 */
Enesim_Text_Engine * enesim_text_engine_default_get(void)
{
#if HAVE_FREETYPE
	return enesim_text_freetype_get();
#endif
	return NULL;
}
