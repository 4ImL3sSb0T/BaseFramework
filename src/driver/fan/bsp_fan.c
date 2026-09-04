#include "bsp_fan.h"

/* TIM15: PSC=240（CubeMX）。APB2 定时器时钟约 240 MHz → tick≈1 MHz
 * ARR=39 → f≈25 kHz，适合多数 4 线 / MOS 驱动风扇 */
#define BSP_FAN_PWM_ARR         39U
#define BSP_FAN_TIM_CHANNEL     TIM_CHANNEL_1

static bool  g_fan_running = false;
static float g_fan_duty    = 0.0f;

static float bsp_fan_clampf(float v, float lo, float hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static uint32_t bsp_fan_duty_to_ccr(float duty)
{
    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(&htim15);
    if (arr == 0U) {
        return 0U;
    }
    /* CCR 允许 0..ARR；duty=1 → 满占空 */
    float ccr_f = duty * (float)arr + 0.5f;
    if (ccr_f <= 0.0f) {
        return 0U;
    }
    if (ccr_f >= (float)arr) {
        return arr;
    }
    return (uint32_t)ccr_f;
}

exit_code_t bsp_fan_init(void)
{
    g_fan_duty = 0.0f;
    g_fan_running = false;

    /* 覆盖 Cube 默认 65535 ARR，得到可用风扇 PWM 频率 */
    __HAL_TIM_SET_AUTORELOAD(&htim15, BSP_FAN_PWM_ARR);
    __HAL_TIM_SET_COMPARE(&htim15, BSP_FAN_TIM_CHANNEL, 0U);

    if (HAL_TIM_PWM_Start(&htim15, BSP_FAN_TIM_CHANNEL) != HAL_OK) {
        return EXIT_HW_FAILURE;
    }

    g_fan_running = true;
    return EXIT_OK;
}

exit_code_t bsp_fan_set_duty(float duty)
{
    duty = bsp_fan_clampf(duty, 0.0f, 1.0f);
    g_fan_duty = duty;

    if (!g_fan_running) {
        if (HAL_TIM_PWM_Start(&htim15, BSP_FAN_TIM_CHANNEL) != HAL_OK) {
            return EXIT_HW_FAILURE;
        }
        g_fan_running = true;
    }

    __HAL_TIM_SET_COMPARE(&htim15, BSP_FAN_TIM_CHANNEL, bsp_fan_duty_to_ccr(duty));
    return EXIT_OK;
}

float bsp_fan_get_duty(void)
{
    return g_fan_duty;
}

exit_code_t bsp_fan_stop(void)
{
    g_fan_duty = 0.0f;
    __HAL_TIM_SET_COMPARE(&htim15, BSP_FAN_TIM_CHANNEL, 0U);

    if (g_fan_running) {
        if (HAL_TIM_PWM_Stop(&htim15, BSP_FAN_TIM_CHANNEL) != HAL_OK) {
            return EXIT_HW_FAILURE;
        }
        g_fan_running = false;
    }
    return EXIT_OK;
}

bool bsp_fan_is_running(void)
{
    return g_fan_running;
}
