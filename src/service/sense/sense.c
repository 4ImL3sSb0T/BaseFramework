#include "sense.h"
#include "driver/adc/bsp_adc.h"

static sense_mode_t g_sense_mode = SENSE_MODE_ADC;

static float sense_adc1_to_volt(uint16_t raw)
{
    return ((float)raw / SENSE_ADC1_RESOLUTION) * SENSE_VOLTAGE_REF;
}

exit_code_t sense_init(sense_mode_t mode)
{
    g_sense_mode = mode;
    return bsp_adc_init();
}

float sense_get_voltage(void)
{
     uint16_t raw = bsp_adc_get_raw_value(BSP_ADC_VOLTAGE_CH);
    return sense_adc1_to_volt(raw) * SENSE_VOLTAGE_FACTOR;
}

float sense_get_current(void)
{
    uint16_t raw;

    switch (g_sense_mode) {
    case SENSE_MODE_ADC: {
        raw = bsp_adc_get_raw_value(BSP_ADC_CURRENT_CH);
        return sense_adc1_to_volt(raw) * SENSE_CURRENT_FACTOR;
    }
    case SENSE_MODE_PAG2ADC: {
        /* ADC2 16-bit PGA 路径，标定后补 */
        raw = bsp_adc_get_raw_value(BSP_ADC_PGA2ADC_CH);
        (void)raw;
        return 0.0f;
    }
    default:
        return 0.0f;
    }
}

float sense_get_temperature(void)
{
    /* 用户约定：温度 = ADC 码值 × 系数 */
    return (float)bsp_adc_get_raw_value(BSP_ADC_TEMP_CH) * SENSE_TEMP_FACTOR;
}

uint16_t sense_get_temperature_raw(void)
{
    return bsp_adc_get_raw_value(BSP_ADC_TEMP_CH);
}
