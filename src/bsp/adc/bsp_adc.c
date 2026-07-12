#include "bsp_adc.h"
#include "opamp.h"

#define ADC_PGA_BUFFER_LENGTH 1
#define ADC_COM_BUFFER_LENGTH 3

/* D-Cache 开启时 DMA 缓冲必须落在 .dma_buf（D2 SRAM, non-cacheable） */
#define ADC_DMA_BUF __attribute__((section(".dma_buf"), aligned(32)))

static ADC_DMA_BUF uint16_t adc_com_buffer[ADC_COM_BUFFER_LENGTH];
static ADC_DMA_BUF uint16_t adc_pga_buffer[ADC_PGA_BUFFER_LENGTH];

exit_code_t bsp_adc_init(void)
{
    if (HAL_OPAMP_Start(&hopamp1) != HAL_OK) {
        return EXIT_HW_FAILURE;
    }
    if (HAL_OPAMP_SelfCalibrate(&hopamp1) != HAL_OK) {
        return EXIT_HW_FAILURE;
    }

    if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED) != HAL_OK) {
        return EXIT_HW_FAILURE;
    }
    if (HAL_ADCEx_Calibration_Start(&hadc2, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED) != HAL_OK) {
        return EXIT_HW_FAILURE;
    }

    /* HAL 要求 uint32_t*，半字 DMA 时底层按 16-bit 写缓冲 */
    if (HAL_ADC_Start_DMA(&hadc1,
                          (uint32_t *)adc_com_buffer,
                          ADC_COM_BUFFER_LENGTH) != HAL_OK) {
        return EXIT_HW_FAILURE;
    }
    if (HAL_ADC_Start_DMA(&hadc2,
                          (uint32_t *)adc_pga_buffer,
                          ADC_PGA_BUFFER_LENGTH) != HAL_OK) {
        return EXIT_HW_FAILURE;
    }

    return EXIT_OK;
}

uint16_t bsp_adc_get_raw_value(bsp_adc_channel_t channel)
{
    switch (channel) {
    case BSP_ADC_VOLTAGE_CH:
        return adc_com_buffer[0];
    case BSP_ADC_CURRENT_CH:
        return adc_com_buffer[1];
    case BSP_ADC_TEMP_CH:
        return adc_com_buffer[2];
    case BSP_ADC_PGA2ADC_CH:
        return adc_pga_buffer[0];
    default:
        return 0U;
    }
}
