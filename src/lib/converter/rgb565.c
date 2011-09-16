#include "Enesim.h"
#include "enesim_private.h"
/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/
static void _2d_rgb565_none_argb8888(Enesim_Buffer_Sw_Data *data, uint32_t dw, uint32_t dh,
		void *sdata, uint32_t sw, uint32_t sh,
		size_t spitch)
{
	uint16_t *dst = data->rgb565.plane0;
	uint32_t *src = sdata;
	size_t dpitch = data->rgb565.plane0_stride;

	while (dh--)
	{
		uint16_t *ddst = dst;
		uint32_t *ssrc = src;
		uint32_t ddw = dw;
		while (ddw--)
		{
			*dst = ((*src & 0xf80000) >> 8) | ((*src & 0xfc00) >> 5)
					| ((*src & 0xf8) >> 3);
			ssrc++;
			ddst++;
		}
		dst += dpitch;
		src += spitch;
	}
}
/*============================================================================*
 *                                 Global                                     *
 *============================================================================*/
void enesim_converter_rgb565_init(void)
{
	/* TODO check if the cpu is the host */
	enesim_converter_surface_register(
			ENESIM_CONVERTER_2D(_2d_rgb565_none_argb8888),
			ENESIM_CONVERTER_RGB565,
			ENESIM_ANGLE_0,
			ENESIM_FORMAT_ARGB8888);
}

