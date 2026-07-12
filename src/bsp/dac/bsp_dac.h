#ifndef BSP_DAC_H
#define BSP_DAC_H

#include "dac.h"
#include "common/tools/common_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/** STM32H7 DAC1 为 12-bit，右对齐有效数据 0..4095 */
#define BSP_DAC_MAX_RAW 4095U

/**
 * @brief 启动 DAC1 CH1（LOADER_REF / PA4），输出 0
 */
exit_code_t bsp_dac_init(void);

/**
 * @brief 停止 DAC1 CH1 输出
 */
exit_code_t bsp_dac_stop(void);

/**
 * @brief 写 12-bit 原始码值并软件触发更新
 * @param raw 0..BSP_DAC_MAX_RAW，超限钳位
 */
exit_code_t bsp_dac_set_raw(uint16_t raw);

/**
 * @brief 读当前 DOR 输出码值
 */
uint16_t bsp_dac_get_raw(void);

/**
 * @brief 通道是否已启动
 */
bool bsp_dac_is_started(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_DAC_H */
