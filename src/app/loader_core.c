#include "loader_core.h"
#include "bsp/tim/bsp_timer.h"
#include "service/load/load_out.h"
#include "loader_config.h"
#include "stdbool.h"

pid_controller_t pid_current = {
    .kp = LOADER_DEFAULT_PID_CURRENT_KP,
    .ki = LOADER_DEFAULT_PID_CURRENT_KI,
    .kd = LOADER_DEFAULT_PID_CURRENT_KD,
    .error_prev = 0.0f,
    .integral = 0.0f,
    .output = 0.0f,
    .output_max = LOADER_DEFAULT_PID_CURRENT_OUTPUT_MAX,
    .output_min = LOADER_DEFAULT_PID_CURRENT_OUTPUT_MIN,
    .integral_max = LOADER_DEFAULT_PID_CURRENT_INTEGRAL_MAX,
    .integral_min = LOADER_DEFAULT_PID_CURRENT_INTEGRAL_MIN,
    .dt = LOADER_DEFAULT_PID_CURRENT_DT
};

pid_controller_t pid_voltage = {
    .kp = LOADER_DEFAULT_PID_VOLTAGE_KP,
    .ki = LOADER_DEFAULT_PID_VOLTAGE_KI,
    .kd = LOADER_DEFAULT_PID_VOLTAGE_KD,
    .error_prev = 0.0f,
    .integral = 0.0f,
    .output = 0.0f,
    .output_max = LOADER_DEFAULT_PID_VOLTAGE_OUTPUT_MAX,
    .output_min = LOADER_DEFAULT_PID_VOLTAGE_OUTPUT_MIN,
    .integral_max = LOADER_DEFAULT_PID_VOLTAGE_INTEGRAL_MAX,
    .integral_min = LOADER_DEFAULT_PID_VOLTAGE_INTEGRAL_MIN,
    .dt = LOADER_DEFAULT_PID_VOLTAGE_DT
};

static void loader_core_loop_control(loader_runtime_t* runtime) {
    switch (runtime->mode) {
    case LOADER_MODE_CC:
        float pid_output = pid_calculate(&pid_current, runtime->current_setpoint, runtime->current_measurement);
        load_out_set_current(pid_output); // Placeholder for PID output
        break;
    case LOADER_MODE_CV:
        float current_target = pid_calculate(&pid_voltage, runtime->voltage_setpoint, runtime->voltage_measurement);
        float out = pid_calculate(&pid_current, current_target, runtime->current_measurement);
        load_out_set_current(out);
        break;
    case LOADER_MODE_CP:
        /* Constant Power mode control logic */
        float current_target = runtime->power_setpoint / (runtime->voltage_measurement > 0 ? runtime->voltage_measurement : 1.0f);
        float out = pid_calculate(&pid_current, current_target, runtime->current_measurement);
        load_out_set_current(out);
        break;
    case LOADER_MODE_CR:
        /* Constant Resistance mode control logic */
        float current_target = runtime->voltage_measurement / (runtime->resistance_setpoint > 0 ? runtime->resistance_setpoint : 1.0f);
        float out = pid_calculate(&pid_current, current_target, runtime->current_measurement);
        load_out_set_current(out);
        break;
    default:
        break;
    }

}

static void loader_core_handle_error(loader_runtime_t* runtime) {
    switch (runtime->error) {
    case LOADER_ERROR_OVERCURRENT:
        /* Handle overcurrent error */
        break;
    case LOADER_ERROR_OVERTEMPERATURE:
        /* Handle overtemperature error */
        break;
    case LOADER_ERROR_UNDERVOLTAGE:
        /* Handle undervoltage error */
        break;
    default:
        break;
    }
}

exit_code_t loader_core_init(void) {
    loader_runtime_init();
    sense_init(SENSE_MODE_ADC);
    load_out_init();
    load_out_enable(true);
    bsp_timer_init();
    bsp_timer_register_callback(loader_core_control_update);
    pid_init(&pid_voltage, LOADER_DEFAULT_PID_VOLTAGE_KP, LOADER_DEFAULT_PID_VOLTAGE_KI, LOADER_DEFAULT_PID_VOLTAGE_KD, LOADER_DEFAULT_PID_VOLTAGE_OUTPUT_MAX, LOADER_DEFAULT_PID_VOLTAGE_INTEGRAL_MAX, LOADER_DEFAULT_PID_VOLTAGE_INTEGRAL_MIN);
    pid_init(&pid_current, LOADER_DEFAULT_PID_CURRENT_KP, LOADER_DEFAULT_PID_CURRENT_KI, LOADER_DEFAULT_PID_CURRENT_KD, LOADER_DEFAULT_PID_CURRENT_OUTPUT_MAX, LOADER_DEFAULT_PID_CURRENT_INTEGRAL_MAX, LOADER_DEFAULT_PID_CURRENT_INTEGRAL_MIN);
    return EXIT_OK;
}

void loader_core_control_update(void) {
    float current = sense_get_current();
    float voltage = sense_get_voltage();

    loader_runtime_t runtime = loader_runtime_get();
    runtime.current_measurement = current;
    runtime.voltage_measurement = voltage;
    runtime.power_measurement = current * voltage; // Calculate power
    runtime.resistance_measurement = (current > 0) ? (voltage / current) : 0.0f; // Calculate resistance

    switch (runtime.state) {
        case LOADER_STATE_RUNNING:
            loader_core_loop_control(&runtime);
            break;
        case LOADER_STATE_IDLE:
        case LOADER_STATE_PAUSED:
            load_out_set(0.0f); // Turn off output when paused
            break;
        case LOADER_STATE_ERROR:
            loader_core_handle_error(&runtime);
            break;
        default:
            load_out_set(0.0f);
            break;
    }
    loader_runtime_set(&runtime);
}

void loader_core_state_update(void *arg) {

}

// 中断触发
void loader_core_fault_tigger(loader_error_t error) {
    loader_runtime_t runtime = loader_runtime_get();
    runtime.error = error;
    runtime.state = LOADER_STATE_ERROR;
    loader_runtime_set(&runtime);
}
