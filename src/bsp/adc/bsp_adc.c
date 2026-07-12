#include "bsp_adc.h"

#define ADC_BUFFER_LENGTH 2
uint16_t adc_buffer[ADC_BUFFER_LENGTH];

exit_code_t bsp_adc_init(void) {
    HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED);
    HAL_ADC_Start_DMA(&hadc1, (uint16_t*)adc_buffer, ADC_BUFFER_LENGTH);
    return EXIT_OK;
}

uint16_t bsp_adc_get_raw_value(bsp_adc_channel_t channel) {
    if (channel >= ADC_BUFFER_LENGTH) {
        return 0; // Invalid channel
    }
    return adc_buffer[channel];
}