#ifndef BSP_ADC_H
#define BSP_ADC_H

#include "adc.h"
#include "lib/tools/common_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * ADC1 扫描通道顺序（与 MX_ADC1_Init Rank 一致）：
 * Rank1 Voltage PA6, Rank2 Current PA7, Rank3 Temp PB1
 * ADC2：PGA 路径单通道
 */
typedef enum {
    BSP_ADC_VOLTAGE_CH = 0,
    BSP_ADC_CURRENT_CH,
    BSP_ADC_TEMP_CH,
    BSP_ADC_PGA2ADC_CH,
    BSP_ADC_CHANNEL_COUNT
} bsp_adc_channel_t;

/** ADC1 12-bit 满量程码值 */
#define BSP_ADC1_MAX_RAW    4095U
/** ADC2 16-bit 满量程码值 */
#define BSP_ADC2_MAX_RAW    65535U

/**
 * @brief 校准并启动 ADC1/ADC2 DMA 循环采样
 */
exit_code_t bsp_adc_init(void);

/**
 * @brief 读取最近一次 DMA 采样 raw
 * @param channel 见 bsp_adc_channel_t；非法返回 0
 */
uint16_t bsp_adc_get_raw_value(bsp_adc_channel_t channel);

#ifdef __cplusplus
}
#endif

#endif /* BSP_ADC_H */
