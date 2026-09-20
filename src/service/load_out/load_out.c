#include "load_out.h"
#include "driver/dac/bsp_dac.h"

static bool  g_enabled   = false;
static float g_out_norm  = 0.0f;
static float g_current_a = 0.0f;

static float load_out_clampf(float v, float lo, float hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static uint16_t load_out_norm_to_raw(float out_norm)
{
    float raw_f = out_norm * LOAD_OUT_RESOLUTION + 0.5f;
    if (raw_f <= 0.0f) {
        return 0U;
    }
    if (raw_f >= LOAD_OUT_RESOLUTION) {
        return BSP_DAC_MAX_RAW;
    }
    return (uint16_t)raw_f;
}

exit_code_t load_out_init(void)
{
    g_enabled   = false;
    g_out_norm  = 0.0f;
    g_current_a = 0.0f;

    exit_code_t ret = bsp_dac_init();
    if (ret != EXIT_OK) {
        return ret;
    }
    return bsp_dac_set_raw(0);
}

exit_code_t load_out_set(float out_norm)
{
    out_norm = load_out_clampf(out_norm, 0.0f, 1.0f);

    if (!g_enabled) {
        g_out_norm  = 0.0f;
        g_current_a = 0.0f;
        return bsp_dac_set_raw(0);
    }

    g_out_norm = out_norm;
    /* 非 set_current 路径：按当前电压反推电流缓存 */
    if (LOAD_OUT_CURRENT_TO_VOLT > 0.0f) {
        g_current_a = (out_norm * LOAD_OUT_VOLTAGE_MAX) / LOAD_OUT_CURRENT_TO_VOLT;
    } else {
        g_current_a = 0.0f;
    }
    return bsp_dac_set_raw(load_out_norm_to_raw(out_norm));
}

exit_code_t load_out_set_ref_voltage(float voltage)
{
    voltage = load_out_clampf(voltage, 0.0f, LOAD_OUT_VOLTAGE_MAX);
    float norm = (LOAD_OUT_VOLTAGE_MAX > 0.0f) ? (voltage / LOAD_OUT_VOLTAGE_MAX) : 0.0f;
    return load_out_set(norm);
}

exit_code_t load_out_set_current(float current)
{
    if (current < 0.0f) {
        current = 0.0f;
    }

    float voltage = current * LOAD_OUT_CURRENT_TO_VOLT;
    exit_code_t ret = load_out_set_ref_voltage(voltage);
    if (ret == EXIT_OK && g_enabled) {
        /* 钳位后按实际输出电压回写，避免与 set 内反推不一致 */
        g_current_a = (LOAD_OUT_CURRENT_TO_VOLT > 0.0f)
                          ? (load_out_get_ref_voltage() / LOAD_OUT_CURRENT_TO_VOLT)
                          : 0.0f;
    } else if (!g_enabled) {
        g_current_a = 0.0f;
    }
    return ret;
}

float load_out_get(void)
{
    return g_out_norm;
}

float load_out_get_ref_voltage(void)
{
    return g_out_norm * LOAD_OUT_VOLTAGE_MAX;
}

float load_out_get_current(void)
{
    return g_current_a;
}

exit_code_t load_out_enable(bool enable)
{
    g_enabled = enable;
    if (!enable) {
        g_out_norm  = 0.0f;
        g_current_a = 0.0f;
        return bsp_dac_set_raw(0);
    }
    return EXIT_OK;
}

bool load_out_is_enabled(void)
{
    return g_enabled;
}

exit_code_t load_out_disable(void)
{
    return load_out_enable(false);
}
