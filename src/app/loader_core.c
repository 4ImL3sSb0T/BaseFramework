#include "loader_core.h"
#include "bsp/tim/bsp_timer.h"
#include "service/load/load_out.h"
#include "service/fan/fan.h"
#include "loader_config.h"

static pid_controller_t pid_current;
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
    pid_reset(&pid_current);
    pid_reset(&pid_voltage);
}

static void loader_core_loop_control(const loader_runtime_t *runtime)
{
    float i_target;
    float out;

    switch (runtime->mode) {
    case LOADER_MODE_CC: {
        i_target = runtime->current_setpoint;
        out = pid_calculate(&pid_current, i_target, runtime->current_measurement);
        (void)load_out_set_current(out);
        break;
    }
    case LOADER_MODE_CV: {
        /* 电压外环 → I_target，电流内环 → 输出电流 */
        i_target = pid_calculate(&pid_voltage,
                                 runtime->voltage_setpoint,
                                 runtime->voltage_measurement);
        out = pid_calculate(&pid_current, i_target, runtime->current_measurement);
        (void)load_out_set_current(out);
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
        out = pid_calculate(&pid_current, i_target, runtime->current_measurement);
        (void)load_out_set_current(out);
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
        out = pid_calculate(&pid_current, i_target, runtime->current_measurement);
        (void)load_out_set_current(out);
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

/** 软件保护：仅判定，关断由 fault_trigger 统一做 */
static bool loader_core_protect_check(const loader_runtime_t *runtime)
{
    if (runtime->current_measurement > LOADER_OVERCURRENT_LIMIT) {
        loader_core_fault_trigger(LOADER_ERROR_OVERCURRENT);
        return true;
    }
    if (runtime->temperature_measurement > LOADER_OVERTEMPERATURE_LIMIT) {
        loader_core_fault_trigger(LOADER_ERROR_OVERTEMPERATURE);
        return true;
    }
    return false;
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
    float current = sense_get_current();
    float voltage = sense_get_voltage();
    float temperature = sense_get_temperature();
    float power;
    float resistance;
    loader_runtime_t runtime;

    power = current * voltage;
    if (loader_core_fabsf(current) > LOADER_CURRENT_EPSILON) {
        resistance = voltage / current;
    } else {
        resistance = 0.0f;
    }

    /* 只写测量，不整结构回写，避免覆盖 UI 设定 */
    loader_runtime_update_measurements(current, voltage, power, resistance, temperature);

    runtime = loader_runtime_get();
    runtime.current_measurement = current;
    runtime.voltage_measurement = voltage;
    runtime.power_measurement = power;
    runtime.resistance_measurement = resistance;
    runtime.temperature_measurement = temperature;

    switch (runtime.state) {
    case LOADER_STATE_RUNNING:
        if (loader_core_protect_check(&runtime)) {
            break;
        }
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
    /* 预留：软定时/请求队列等；启停走 request_* API */
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
