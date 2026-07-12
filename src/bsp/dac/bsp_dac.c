#include "bsp_dac.h"

static bool g_dac_started = false;
static uint16_t g_dac_raw = 0;

static void bsp_dac_software_trigger(void)
{
    /* CubeMX: DAC_TRIGGER_SOFTWARE — DHR→DOR 需 SWTRIG */
    SET_BIT(hdac1.Instance->SWTRIGR, DAC_SWTRIGR_SWTRIG1);
}

exit_code_t bsp_dac_init(void)
{
    g_dac_raw = 0;

    if (HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 0U) != HAL_OK) {
        return EXIT_HW_FAILURE;
    }
    if (HAL_DAC_Start(&hdac1, DAC_CHANNEL_1) != HAL_OK) {
        return EXIT_HW_FAILURE;
    }

    g_dac_started = true;
    return EXIT_OK;
}

exit_code_t bsp_dac_stop(void)
{
    if (HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 0U) != HAL_OK) {
        return EXIT_HW_FAILURE;
    }
    if (g_dac_started) {
        bsp_dac_software_trigger();
    }
    if (HAL_DAC_Stop(&hdac1, DAC_CHANNEL_1) != HAL_OK) {
        return EXIT_HW_FAILURE;
    }

    g_dac_raw = 0;
    g_dac_started = false;
    return EXIT_OK;
}

exit_code_t bsp_dac_set_raw(uint16_t raw)
{
    if (!g_dac_started) {
        return EXIT_NOT_INITIALIZED;
    }

    if (raw > BSP_DAC_MAX_RAW) {
        raw = BSP_DAC_MAX_RAW;
    }

    if (HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, (uint32_t)raw) != HAL_OK) {
        return EXIT_HW_FAILURE;
    }
    bsp_dac_software_trigger();

    g_dac_raw = raw;
    return EXIT_OK;
}

uint16_t bsp_dac_get_raw(void)
{
    if (!g_dac_started) {
        return 0;
    }
    return g_dac_raw;
}

bool bsp_dac_is_started(void)
{
    return g_dac_started;
}
