#ifndef BSP_FAN_H
#define BSP_FAN_H

#include "tim.h"
#include "lib/tools/common_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 风扇 PWM：TIM15 CH1 → PE5（Cube 标签 FUN_PWM）
 * Cube 默认 ARR=65535 频率过低，init 时改为约 25 kHz 的 ARR。
 */

/**
 * @brief 配置 ARR、占空比 0，启动 PWM 通道（输出仍为 0）
 */
exit_code_t bsp_fan_init(void);

/**
 * @brief 设置占空比
 * @param duty 0.0 ~ 1.0，超限钳位
 */
exit_code_t bsp_fan_set_duty(float duty);

/**
 * @brief 当前占空比（软件缓存，非读回 CCR）
 */
float bsp_fan_get_duty(void);

/**
 * @brief 停止 PWM 输出（CCR=0 并 Stop）
 */
exit_code_t bsp_fan_stop(void);

/**
 * @brief 是否已 Start PWM
 */
bool bsp_fan_is_running(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_FAN_H */
