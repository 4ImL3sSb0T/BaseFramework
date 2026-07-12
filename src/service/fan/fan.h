#ifndef FAN_H
#define FAN_H

#include "bsp/fan/bsp_fan.h"
#include "common/tools/common_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 风扇领域接口（电子负载散热）
 * - 速度用 0~1 归一化（或 0~100% 的 set_percent）
 * - enable=false 时强制停转
 */

/**
 * @brief 初始化底层 PWM，默认关闭输出
 */
exit_code_t fan_init(void);

/**
 * @brief 允许/禁止风扇；禁止时占空比强制 0
 */
exit_code_t fan_enable(bool enable);

bool fan_is_enabled(void);

/**
 * @brief 设定转速（归一化 0.0~1.0）；未 enable 时只缓存目标
 */
exit_code_t fan_set_speed(float speed);

/**
 * @brief 设定转速百分比 0~100
 */
exit_code_t fan_set_percent(float percent);

/** 当前生效占空比 0~1（未 enable 时为 0） */
float fan_get_speed(void);

/** 目标速度（忽略 enable） */
float fan_get_target_speed(void);

#ifdef __cplusplus
}
#endif

#endif /* FAN_H */
