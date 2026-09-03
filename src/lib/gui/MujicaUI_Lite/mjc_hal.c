/*
 * mjc_hal.c — HAL registry (board drivers bind via mjc_hal_use_driver).
 * IPS200/114 paths removed; H750 uses mjc_hal_gfx.cpp.
 */

#include "mjc_hal.h"
#include <stddef.h>

static mjc_hal_driver_t *g_active_driver = NULL;

uint8_t mjc_hal_use_driver(mjc_hal_driver_t *driver)
{
    if (driver == NULL) {
        return 0;
    }
    g_active_driver = driver;
    return 1;
}

uint8_t mjc_hal_init(const mjc_hal_config_t *config)
{
    mjc_hal_config_t cfg = mjc_hal_get_default_config();
    if (config != NULL) {
        cfg = *config;
    }

    switch (cfg.driver_type) {
    case MJC_DRIVER_CUSTOM:
        if (cfg.custom_driver == NULL) {
            /* custom_driver filled later by mjc_hal_gfx_init / use_driver */
            return (g_active_driver != NULL) ? 1 : 0;
        }
        return mjc_hal_use_driver(cfg.custom_driver);
    case MJC_DRIVER_IPS200:
    case MJC_DRIVER_IPS114:
    default:
        return 0;
    }
}

const mjc_hal_driver_t *mjc_hal_get_driver(void)
{
    return g_active_driver;
}

uint16_t mjc_hal_screen_width(void)
{
    return g_active_driver ? g_active_driver->width : 0;
}

uint16_t mjc_hal_screen_height(void)
{
    return g_active_driver ? g_active_driver->height : 0;
}

void mjc_hal_present(void)
{
    if (g_active_driver != NULL && g_active_driver->present != NULL) {
        g_active_driver->present();
    }
}
