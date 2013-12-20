#include "enesim_example_renderer.h"

/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/
typedef struct _Enesim_Example_Renderer_Backend
{
	const char *name;
	Enesim_Example_Renderer_Backend_Interface *iface;
} Enesim_Example_Renderer_Backend;

static Enesim_Example_Renderer_Backend backends[] = {
	{ "image", &enesim_example_renderer_backend_image },
#if BUILD_OPENGL
#if BUILD_GLX
	{ "glx", &enesim_example_renderer_backend_glx },
#endif
#if BUILD_WGL
	{ "wgl", &enesim_example_renderer_backend_wgl },
#endif
#endif
};

static Enesim_Example_Renderer_Backend *backend = &backends[0];

static void help(const char *name)
{
	unsigned int i;
	printf("Usage: %s [BACKEND ROP]]\n", name);
	printf("Where BACKEND can be one of the following:\n");
	
	for (i = 0; i < sizeof(backends)/sizeof(Enesim_Example_Renderer_Backend); i++)
	{
		printf("- %s\n", backends[i].name);
	}
	printf("Where ROP can be \"blend\" or \"fill\"\n");
}

static Eina_Bool parse_options(Enesim_Example_Renderer_Options *options,
		int argc, char **argv)
{
	/* default options */
	options->name = basename(argv[0]);
	options->rop = ENESIM_ROP_FILL;
	options->width = 256;
	options->height = 256;

	/* handle the parameters */
	if (argc > 1)
	{
		if (argc < 3)
		{
			help(options->name);
			return EINA_FALSE;
		}
		/* backend */
		{
			Eina_Bool found = EINA_FALSE;
			unsigned int i;
			for (i = 0; i < sizeof(backends)/sizeof(Enesim_Example_Renderer_Backend); i++)
			{
				if (!strcmp(backends[i].name, argv[1]))
				{
					backend = &backends[i];
					found = EINA_TRUE;
				}
			}
			if (!found)
			{
				printf("backend not found\n");
				help(options->name);
				return EINA_FALSE;
			}
		}
		
		/* the rop */
		if (!strcmp(argv[2], "fill"))
			options->rop = ENESIM_ROP_FILL;
		else if (!strcmp(argv[2], "blend"))
			options->rop = ENESIM_ROP_BLEND;
		else
		{
			help(options->name);
			return EINA_FALSE;
		}
	}
	return EINA_TRUE;
}

/*============================================================================*
 *                                 Global                                     *
 *============================================================================*/
void enesim_example_renderer_draw(Enesim_Renderer *r, Enesim_Surface *s,
		Enesim_Example_Renderer_Options *options)
{
	Enesim_Renderer *c;
	Enesim_Log *error = NULL;
	Eina_Rectangle bounds;

	if (!r)
	{
		printf("No such renderer, nothing to do");
		return;
	}

	if (!s)
	{
		printf("No surface, nothing to do");
		enesim_renderer_unref(r);
		return;
	}

	enesim_renderer_destination_bounds_get(r, &bounds, 0, 0);
	printf("bounds %" EINA_RECTANGLE_FORMAT "\n", 	
			EINA_RECTANGLE_ARGS(&bounds));
	c = enesim_renderer_checker_new();
	enesim_renderer_checker_odd_color_set(c, 0xffcccccc);
	enesim_renderer_checker_even_color_set(c, 0xffaaaaaa);
	enesim_renderer_checker_width_set(c, 10);
	enesim_renderer_checker_height_set(c, 10);
	{
		Enesim_Matrix m;
		enesim_matrix_rotate(&m, 20);
		enesim_renderer_transformation_set(c, &m);
	}
	/* first draw the background */	
	if (!enesim_renderer_draw(c, s, ENESIM_ROP_FILL, NULL, 0, 0,
			&error))
	{
		enesim_log_dump(error);
	}
	/* now the real renderer */
	if (!enesim_renderer_draw(r, s, options->rop, NULL, 0, 0,
			&error))
	{
		enesim_log_dump(error);
	}

	enesim_renderer_unref(r);
	enesim_renderer_unref(c);
}

int main(int argc, char **argv)
{
	Enesim_Example_Renderer_Options options;
	Enesim_Renderer *r;

	if (!parse_options(&options, argc, argv))
		return 1;
	if (!backend)
		return 1;

	enesim_init();
	if (!backend->iface->setup(&options))
	{
		printf("The backend failed\n");
		enesim_shutdown();
		return 1;
	}
	r = enesim_example_renderer_renderer_get(&options);
	backend->iface->run(r, &options);
	backend->iface->cleanup();

	enesim_shutdown();
	return 0;
}
