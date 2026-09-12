#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include <stdint.h>
#include <stdbool.h>
#include "lib/tools/common_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * UI / 用户按键 ID（与 multi_button button_id 对齐）
 * 硬件：CubeMX 已配置上拉输入，按下为低电平
 *   UI_UP   PC8
 *   UI_DOWN PD14
 *   UI_ENT  PC7
 *   UI_BACK PC6
 *   USR_KEY PC13
 */
typedef enum {
    BSP_KEY_UI_UP = 0,
    BSP_KEY_UI_DOWN,
    BSP_KEY_UI_ENT,
    BSP_KEY_UI_BACK,
    BSP_KEY_USR,
    BSP_KEY_COUNT
} bsp_key_id_t;

/** 按键有效电平（按下）：0 = 低有效 */
#define BSP_KEY_ACTIVE_LEVEL  0u

/**
 * @brief 可选：Cube 已在 MX_GPIO_Init 配置引脚，此处为空操作
 */
exit_code_t bsp_gpio_init(void);

/**
 * @brief 读按键原始 GPIO 电平（0/1）
 * @param key_id bsp_key_id_t
 * @return 0 或 1；非法 id 返回非按下电平
 */
uint8_t bsp_gpio_key_level(uint8_t key_id);

/**
 * @brief 是否处于按下（考虑 active_level）
 */
bool bsp_gpio_key_is_pressed(bsp_key_id_t key_id);

#ifdef __cplusplus
}
#endif

#endif /* BSP_GPIO_H */
