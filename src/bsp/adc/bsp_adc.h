#include "adc.h"
#include "common/tools/common_def.h"

typedef enum {
    BSP_ADC_VOLTAGE_CH = 0,
    BSP_ADC_CURRENT_CH,
    BSP_ADC_TEMP_CH,
    BSP_ADC_PGA2ADC_CH,
    BSP_ADC_CHANNEL_COUNT
} bsp_adc_channel_t;

exit_code_t bsp_adc_init(void);
uint16_t bsp_adc_get_raw_value(bsp_adc_channel_t channel);
