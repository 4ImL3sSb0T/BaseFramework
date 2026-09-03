#ifndef __MJC_CONFIG_H
#define __MJC_CONFIG_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct mjc_hal_driver_t;

typedef enum {
    MJC_DRIVER_CUSTOM = 0, /* Adafruit GFX / tft_fb on this project */
    MJC_DRIVER_IPS200,     /* legacy (not built on H750) */
    MJC_DRIVER_IPS114,
} mjc_driver_type_t;

typedef enum {
    MJC_DIR_AUTO = 0,
    MJC_DIR_PORTRAIT,
    MJC_DIR_PORTRAIT_180,
    MJC_DIR_LANDSCAPE,
    MJC_DIR_LANDSCAPE_180,
} mjc_display_dir_t;

typedef struct {
    mjc_driver_type_t driver_type;
    mjc_display_dir_t display_dir;
    struct mjc_hal_driver_t *custom_driver;
} mjc_hal_config_t;

/* ST7735 landscape after rotation=1: 160 x 128
 * Adafruit built-in font cell 6x8 @ setTextSize(1)
 */
#define MJC_RENDER_FONT_WIDTH    6
#define MJC_RENDER_FONT_HEIGHT   8
#define MJC_RENDER_PADDING_X     2
#define MJC_RENDER_GAP_X         4
#define MJC_RENDER_VALUE_BUF     24
#define MJC_RENDER_NAME_BUF      32
#define MJC_RENDER_COLOR_NORMAL_PEN     0xFFFF  /* white */
#define MJC_RENDER_COLOR_NORMAL_BG      0x0000  /* black */
#define MJC_RENDER_COLOR_SELECTED_PEN   0x0000  /* black */
#define MJC_RENDER_COLOR_SELECTED_BG    0x07E0  /* green */

#define MJC_RENDER_MAX_TRACKED_ITEMS    32

#define MJC_HAL_CONFIG_GFX_DEFAULT \
    { MJC_DRIVER_CUSTOM, MJC_DIR_LANDSCAPE, NULL }

static inline mjc_hal_config_t mjc_hal_get_default_config(void)
{
    const mjc_hal_config_t cfg = MJC_HAL_CONFIG_GFX_DEFAULT;
    return cfg;
}

#ifdef __cplusplus
}
#endif

#endif /* __MJC_CONFIG_H */
