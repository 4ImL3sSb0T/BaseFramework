#ifndef SENSE_H
#define SENSE_H

#include <stdint.h>
#include "lib/tools/common_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 采样物理量：电压 / 电流 / 温度
 * - ADC1：12-bit，通道见 bsp_adc
 * - 温度：T = raw * SENSE_TEMP_FACTOR（按硬件标定系数）
 */

#define SENSE_VOLTAGE_REF           3.3f
/** ADC1 12-bit 满量程 */
#define SENSE_ADC1_RESOLUTION       4095.0f

#define SENSE_VOLTAGE_FACTOR        11.2619f   /* 按分压/前端增益标定 */
#define SENSE_CURRENT_FACTOR        2.0f    /* 按分流/运放增益标定 */
//4.730 0.42
/**
 * 温度标定：T(°C) = ADC_raw * SENSE_TEMP_FACTOR
 * 按传感器/分压实测改此系数即可，无需再除满量程。
 */
#define SENSE_TEMP_FACTOR           0.1f

typedef enum {
    SENSE_MODE_ADC,
    SENSE_MODE_PAG2ADC
} sense_mode_t;

exit_code_t sense_init(sense_mode_t mode);

float sense_get_voltage(void);
float sense_get_current(void);

/**
 * @brief 负载温度 (°C)，raw * SENSE_TEMP_FACTOR
 */
float sense_get_temperature(void);

/** 调试用：温度通道 raw */
uint16_t sense_get_temperature_raw(void);

#ifdef __cplusplus
}
#endif

#endif /* SENSE_H */
