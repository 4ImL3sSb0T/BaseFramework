#include "fan.h"
#include "board/fan/bsp_fan.h"

static bool  g_enabled       = false;
static float g_target_speed  = 0.0f;

static float fan_clampf(float v, float lo, float hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static exit_code_t fan_apply(void)
{
    if (!g_enabled) {
        return bsp_fan_set_duty(0.0f);
    }
    return bsp_fan_set_duty(g_target_speed);
}

exit_code_t fan_init(void)
{
    g_enabled = false;
    g_target_speed = 0.0f;

    exit_code_t ret = bsp_fan_init();
    if (ret != EXIT_OK) {
        return ret;
    }
    return fan_apply();
}

exit_code_t fan_enable(bool enable)
{
    g_enabled = enable;
    return fan_apply();
}

bool fan_is_enabled(void)
{
    return g_enabled;
}

exit_code_t fan_set_speed(float speed)
{
    g_target_speed = fan_clampf(speed, 0.0f, 1.0f);
    return fan_apply();
}

exit_code_t fan_set_percent(float percent)
{
    return fan_set_speed(percent / 100.0f);
}

float fan_get_speed(void)
{
    if (!g_enabled) {
        return 0.0f;
    }
    return bsp_fan_get_duty();
}

float fan_get_target_speed(void)
{
    return g_target_speed;
}
