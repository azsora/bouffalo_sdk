#ifndef __ILI9806E_KD050FWFIA019_DSI_H__
#define __ILI9806E_KD050FWFIA019_DSI_H__

#include "../lcd_conf.h"
#include "stdint.h"

#if defined(LCD_DSI_ILI9806E_KD050FWFIA019)

#include "mipi_dsi_v2.h"

/* KD050FWFIA019 panel: 480x854, ILI9806E driver IC, 2-lane MIPI DSI (RGB565) */

/* Panel orientation (ILI9806E_KD050FWFIA019_ROTATE_180) is a board-mounting
 * property, so it is configured in lcd_conf_user.h (default in lcd_conf.h),
 * not hard-coded here. Consumed by ili9806e_kd050fwfia019_dsi.c via #if. */

/* Do not modify the following */

#define ILI9806E_KD050FWFIA019_DSI_W           480
#define ILI9806E_KD050FWFIA019_DSI_H           854
#define ILI9806E_KD050FWFIA019_DSI_COLOR_DEPTH 32

typedef uint32_t ili9806e_kd050fwfia019_dsi_color_t;

/* Turn 24-bit RGB color to 16-bit */
#define RGB(r, g, b) (((r >> 3) << 3 | (g >> 5) | (g >> 2) << 13 | (b >> 3) << 8) & 0xffff)
/* Calculate 32-bit or 16-bit absolute value */
#define ABS32(value) ((value ^ (value >> 31)) - (value >> 31))
#define ABS16(value) ((value ^ (value >> 15)) - (value >> 15))

int ili9806e_kd050fwfia019_dsi_init(ili9806e_kd050fwfia019_dsi_color_t *screen_buffer);
int ili9806e_kd050fwfia019_dsi_screen_switch(ili9806e_kd050fwfia019_dsi_color_t *screen_buffer);
ili9806e_kd050fwfia019_dsi_color_t *ili9806e_kd050fwfia019_dsi_get_screen_using(void);
int ili9806e_kd050fwfia019_dsi_frame_callback_register(uint32_t callback_type, void (*callback)(void));
const mipi_dsi_v2_timing_t *ili9806e_kd050fwfia019_dsi_get_timing(void);
int display_prepare(void);
int display_enable(void);
int display_disable(void);
int display_unprepare(void);

#endif

#endif /* __ILI9806E_KD050FWFIA019_DSI_H__ */
