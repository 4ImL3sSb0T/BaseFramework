#ifndef LOAD_OUT_H
#define LOAD_OUT_H

#include <stdbool.h>
#include "lib/tools/common_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 负载执行器（LOADER_REF）
 * - core 只输出 out_norm ∈ [0, 1]
 * - 本层完成归一化/电压 → DAC 码值映射
 * 标定系数按板级硬件调整。
 */

/** DAC 参考电压 (V)，与 VDDA 一致 */
#define LOAD_OUT_VREF           3.3f
/** 12-bit 满量程 */
#define LOAD_OUT_RESOLUTION     4095.0f
/** 允许设定的最大输出电压 (V)，默认等于 VREF */
#define LOAD_OUT_VOLTAGE_MAX    3.3f
/**
 * 电流 → 基准电压 系数 (V/A)
 * V_ref = I_set * LOAD_OUT_CURRENT_TO_VOLT
 * 按功率级分流/运放增益标定，默认 1.0 即 1A → 1V
 */
#define LOAD_OUT_CURRENT_TO_VOLT 1.0f

/**
 * @brief 初始化执行器（启动 DAC，输出 0）
 */
exit_code_t load_out_init(void);

/**
 * @brief 按归一化值设定输出
 * @param out_norm 0.0 ~ 1.0，超限钳位；未 enable 时强制 0
 */
exit_code_t load_out_set(float out_norm);

/**
 * @brief 按电压设定输出 (V)
 * @param voltage 0 ~ LOAD_OUT_VOLTAGE_MAX
 */
exit_code_t load_out_set_voltage(float voltage);

/**
 * @brief 按电流设定输出 (A)
 * @param current 设定电流；内部 V = I * LOAD_OUT_CURRENT_TO_VOLT，再走 set_voltage
 */
exit_code_t load_out_set_current(float current);

/**
 * @brief 读取最近一次设定的归一化值（非硬件读回）
 */
float load_out_get(void);

/**
 * @brief 读取最近一次设定电压 (V)
 */
float load_out_get_voltage(void);

/**
 * @brief 读取最近一次设定电流 (A)
 */
float load_out_get_current(void);

/**
 * @brief 允许/禁止输出（禁止时强制 DAC=0）
 */
exit_code_t load_out_enable(bool enable);

/**
 * @brief 当前是否允许输出
 */
bool load_out_is_enabled(void);

/**
 * @brief 关断：输出 0 并清除 enable
 */
exit_code_t load_out_disable(void);

#ifdef __cplusplus
}
#endif

#endif /* LOAD_OUT_H */
