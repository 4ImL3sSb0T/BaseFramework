#include "loader_core.h"
#include "board/tim/bsp_timer.h"
#include "board/load/load_out.h"
#include "board/fan/fan.h"
#include "loader_config.h"

#if LOADER_USE_SOFTWARE_CURRENT_PID
static pid_controller_t pid_current;
#endif
static pid_controller_t pid_voltage;

static float loader_core_clampf(float v, float lo, float hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static float loader_core_fabsf(float v)
{
    return (v < 0.0f) ? -v : v;
}

static void loader_core_output_off(void)
{
    (void)load_out_set(0.0f);
    (void)load_out_disable();
}

static void loader_core_pid_reset_all(void)
{
#if LOADER_USE_SOFTWARE_CURRENT_PID
    pid_reset(&pid_current);
#endif
    pid_reset(&pid_voltage);
}

/**
 * 将电流目标下发到功率级
 * 软件电流环：PID 校正后输出；硬件运放环：直接下发 I_target
 */
static void loader_core_apply_current(float i_target, float i_measurement)
{
#if LOADER_USE_SOFTWARE_CURRENT_PID
    float out = pid_calculate(&pid_current, i_target, i_measurement);
    (void)load_out_set_current(out);
#else
    (void)i_measurement;
    (void)load_out_set_current(i_target);
#endif
}

static void loader_core_loop_control(const loader_runtime_t *runtime)
{
    float i_target = 0.0f;

    switch (runtime->mode) {
    case LOADER_MODE_CC: {
        i_target = runtime->current_setpoint;
        loader_core_apply_current(i_target, runtime->current_measurement);
        break;
    }
    case LOADER_MODE_CV: {
        /* 电压外环 → I_target；电流由软件 PID 或外部运放闭环 */
        i_target = pid_calculate(&pid_voltage,
                                 runtime->voltage_setpoint,
                                 runtime->voltage_measurement);
        loader_core_apply_current(i_target, runtime->current_measurement);
        break;
    }
    case LOADER_MODE_CP: {
        /* I_target = P_set / max(V_meas, ε) */
        float v = runtime->voltage_measurement;
        if (v < LOADER_VOLTAGE_EPSILON) {
            v = LOADER_VOLTAGE_EPSILON;
        }
        i_target = runtime->power_setpoint / v;
        i_target = loader_core_clampf(i_target, 0.0f, LOADER_CURRENT_MAX);
        loader_core_apply_current(i_target, runtime->current_measurement);
        break;
    }
    case LOADER_MODE_CR: {
        /* I_target = V_meas / max(R_set, ε) */
        float r = runtime->resistance_setpoint;
        if (r < LOADER_RESISTANCE_EPSILON) {
            r = LOADER_RESISTANCE_EPSILON;
        }
        i_target = runtime->voltage_measurement / r;
        i_target = loader_core_clampf(i_target, 0.0f, LOADER_CURRENT_MAX);
        loader_core_apply_current(i_target, runtime->current_measurement);
        break;
    }
    default:
        loader_core_output_off();
        break;
    }
}

static void loader_core_handle_error(const loader_runtime_t *runtime)
{
    (void)runtime;
    /* 任意故障：功率级必须关断 */
    loader_core_output_off();
}

exit_code_t loader_core_init(void)
{
    exit_code_t ret;

    ret = loader_runtime_init();
    if (ret != EXIT_OK) {
        return ret;
    }

    ret = sense_init(SENSE_MODE_ADC);
    if (ret != EXIT_OK) {
        return ret;
    }

    ret = load_out_init();
    if (ret != EXIT_OK) {
        return ret;
    }

    ret = fan_init();
    if (ret != EXIT_OK) {
        return ret;
    }

    /* 上电默认禁止输出，等 request_run 再 enable */
    (void)load_out_enable(false);

    /* pid_init(pid, kp, ki, kd, dt, output_min, output_max) */
#if LOADER_USE_SOFTWARE_CURRENT_PID
    pid_init(&pid_current,
             LOADER_DEFAULT_PID_CURRENT_KP,
             LOADER_DEFAULT_PID_CURRENT_KI,
             LOADER_DEFAULT_PID_CURRENT_KD,
             LOADER_DEFAULT_PID_CURRENT_DT,
             LOADER_DEFAULT_PID_CURRENT_OUTPUT_MIN,
             LOADER_DEFAULT_PID_CURRENT_OUTPUT_MAX);
    pid_set_integral_limits(&pid_current,
                            LOADER_DEFAULT_PID_CURRENT_INTEGRAL_MIN,
                            LOADER_DEFAULT_PID_CURRENT_INTEGRAL_MAX);
#endif

    pid_init(&pid_voltage,
             LOADER_DEFAULT_PID_VOLTAGE_KP,
             LOADER_DEFAULT_PID_VOLTAGE_KI,
             LOADER_DEFAULT_PID_VOLTAGE_KD,
             LOADER_DEFAULT_PID_VOLTAGE_DT,
             LOADER_DEFAULT_PID_VOLTAGE_OUTPUT_MIN,
             LOADER_DEFAULT_PID_VOLTAGE_OUTPUT_MAX);
    pid_set_integral_limits(&pid_voltage,
                            LOADER_DEFAULT_PID_VOLTAGE_INTEGRAL_MIN,
                            LOADER_DEFAULT_PID_VOLTAGE_INTEGRAL_MAX);

    ret = bsp_timer_init();
    if (ret != EXIT_OK) {
        return ret;
    }

    ret = bsp_timer_register_callback(loader_core_control_update);
    if (ret != EXIT_OK) {
        return ret;
    }

    return EXIT_OK;
}

void loader_core_control_update(void)
{
// 如果有运放作为电流环, 则直接使用获得电流, 已经有闭环控制
#if LOADER_USE_SOFTWARE_CURRENT_PID
    float current = sense_get_current();
#else
    float current = load_out_get_current();
#endif
    float voltage = sense_get_voltage();
    float temperature = sense_get_temperature();
    float resistance = 0.0f;

    float power = current * voltage;
    if (loader_core_fabsf(current) > LOADER_CURRENT_EPSILON)
        resistance = voltage / current;

    /* 只写测量，不整结构回写，避免覆盖 UI 设定 */
    loader_runtime_update_measurements(current, voltage, power, resistance, temperature);

    loader_runtime_t runtime = loader_runtime_get();
    runtime.current_measurement = current;
    runtime.voltage_measurement = voltage;
    runtime.power_measurement = power;
    runtime.resistance_measurement = resistance;
    runtime.temperature_measurement = temperature;

    switch (runtime.state) {
    case LOADER_STATE_RUNNING:
        loader_core_loop_control(&runtime);
        break;

    case LOADER_STATE_IDLE:
    case LOADER_STATE_PAUSED:
        loader_core_output_off();
        break;

    case LOADER_STATE_ERROR:
        loader_core_handle_error(&runtime);
        break;

    default:
        loader_core_output_off();
        break;
    }
}

void loader_core_state_update(void *arg)
{
    (void)arg;
    loader_runtime_t runtime = loader_runtime_get();

    /* 已在故障则只保持关断语义由 control_update 保证，避免重复 enter */
    if (runtime.state == LOADER_STATE_ERROR) {
        return;
    }

    if (runtime.current_measurement > LOADER_OVERCURRENT_LIMIT) {
        loader_core_fault_trigger(LOADER_ERROR_OVERCURRENT);
        return;
    }
    if (runtime.temperature_measurement > LOADER_OVERTEMPERATURE_LIMIT) {
        loader_core_fault_trigger(LOADER_ERROR_OVERTEMPERATURE);
        return;
    }
}

exit_code_t loader_core_request_run(void)
{
    loader_runtime_t runtime = loader_runtime_get();

    if (runtime.state == LOADER_STATE_ERROR) {
        return EXIT_BUSY;
    }
    if (runtime.state != LOADER_STATE_IDLE && runtime.state != LOADER_STATE_PAUSED) {
        return EXIT_OK; /* 已在跑 */
    }

    loader_core_pid_reset_all();
    loader_runtime_set_state(LOADER_STATE_RUNNING);
    (void)load_out_enable(true);
    return EXIT_OK;
}

exit_code_t loader_core_request_stop(void)
{
    loader_runtime_t runtime = loader_runtime_get();

    if (runtime.state == LOADER_STATE_ERROR) {
        /* 故障态先 clear_fault 再 run；stop 仍保证关断 */
        loader_core_output_off();
        return EXIT_OK;
    }

    loader_core_output_off();
    loader_core_pid_reset_all();
    loader_runtime_set_state(LOADER_STATE_IDLE);
    return EXIT_OK;
}

exit_code_t loader_core_clear_fault(void)
{
    loader_runtime_t runtime = loader_runtime_get();

    if (runtime.state != LOADER_STATE_ERROR) {
        return EXIT_OK;
    }

    loader_core_output_off();
    loader_core_pid_reset_all();
    loader_runtime_clear_fault();
    return EXIT_OK;
}

void loader_core_fault_trigger(loader_error_t error)
{
    loader_core_output_off();
    loader_core_pid_reset_all();
    loader_runtime_enter_fault(error);
}
