#include "bsp_adc.h"
#include "opamp.h"

#define ADC_PGA_BUFFER_LENGTH 1
#define ADC_COM_BUFFER_LENGTH 2
uint16_t adc_com_buffer[ADC_COM_BUFFER_LENGTH];
uint16_t adc_pga_buffer[ADC_PGA_BUFFER_LENGTH];

exit_code_t bsp_adc_init(void) {
    HAL_OPAMP_Start(&hopamp1);
    HAL_OPAMP_SelfCalibrate(&hopamp1);
    HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED);
    HAL_ADCEx_Calibration_Start(&hadc2, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED);
    HAL_ADC_Start_DMA(&hadc1, (uint16_t*)adc_com_buffer, ADC_COM_BUFFER_LENGTH);
    HAL_ADC_Start_DMA(&hadc2, (uint16_t*)adc_pga_buffer, ADC_PGA_BUFFER_LENGTH);
    return EXIT_OK;
}

uint16_t bsp_adc_get_raw_value(bsp_adc_channel_t channel) {
    if (channel >= BSP_ADC_CHANNEL_COUNT) {
        return 0; // Invalid channel
    }
    switch (channel) {
        case BSP_ADC_VOLTAGE_CH:
            return adc_com_buffer[0];
        case BSP_ADC_CURRENT_CH:
            return adc_com_buffer[1];
        case BSP_ADC_PGA2ADC_CH:
            return adc_pga_buffer[0];
        default:
            return 0;
    }
}