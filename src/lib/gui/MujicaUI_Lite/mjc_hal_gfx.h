#ifndef __MJC_HAL_GFX_H
#define __MJC_HAL_GFX_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Init ST7735 frame buffer (if needed) and register as MujicaUI HAL driver.
 * @return 1 success, 0 fail
 */
uint8_t mjc_hal_gfx_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __MJC_HAL_GFX_H */
