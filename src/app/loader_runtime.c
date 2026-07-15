#include "loader_runtime.h"
#include "loader_config.h"
#include "FreeRTOS.h"
#include "task.h"

static loader_runtime_t g_loader_runtime = {
    .state = LOADER_STATE_IDLE,
    .error = LOADER_ERROR_NONE,
    .mode = LOADER_MODE_CC,
    .current_setpoint = LOADER_DEFAULT_CURRENT_SETPOINT,
    .current_measurement = 0.0f,
    .voltage_setpoint = LOADER_DEFAULT_VOLTAGE_SETPOINT,
    .voltage_measurement = 0.0f,
    .power_setpoint = LOADER_DEFAULT_POWER_SETPOINT,
    .power_measurement = 0.0f,
    .resistance_setpoint = LOADER_DEFAULT_RESISTANCE_SETPOINT,
    .resistance_measurement = 0.0f,
    .temperature_measurement = 0.0f,
    .temperature_setpoint = 0.0f
};

/* 控制环在 TIM ISR 中跑，UI 在任务中改设定：用临界区代替互斥量 */
static void loader_runtime_enter(UBaseType_t *ux_saved, BaseType_t *in_isr)
{
    *in_isr = (BaseType_t)xPortIsInsideInterrupt();
    if (*in_isr != pdFALSE) {
        *ux_saved = taskENTER_CRITICAL_FROM_ISR();
    } else {
        taskENTER_CRITICAL();
        *ux_saved = 0U;
    }
}

static void loader_runtime_exit(UBaseType_t ux_saved, BaseType_t in_isr)
{
    if (in_isr != pdFALSE) {
        taskEXIT_CRITICAL_FROM_ISR(ux_saved);
    } else {
        taskEXIT_CRITICAL();
    }
}

exit_code_t loader_runtime_init(void)
{
    UBaseType_t ux_saved;
    BaseType_t in_isr;

    loader_runtime_enter(&ux_saved, &in_isr);
    g_loader_runtime.state = LOADER_STATE_IDLE;
    g_loader_runtime.error = LOADER_ERROR_NONE;
    g_loader_runtime.mode = LOADER_MODE_CC;
    g_loader_runtime.current_setpoint = LOADER_DEFAULT_CURRENT_SETPOINT;
    g_loader_runtime.voltage_setpoint = LOADER_DEFAULT_VOLTAGE_SETPOINT;
    g_loader_runtime.power_setpoint = LOADER_DEFAULT_POWER_SETPOINT;
    g_loader_runtime.resistance_setpoint = LOADER_DEFAULT_RESISTANCE_SETPOINT;
    g_loader_runtime.current_measurement = 0.0f;
    g_loader_runtime.voltage_measurement = 0.0f;
    g_loader_runtime.power_measurement = 0.0f;
    g_loader_runtime.resistance_measurement = 0.0f;
    g_loader_runtime.temperature_measurement = 0.0f;
    loader_runtime_exit(ux_saved, in_isr);
    return EXIT_OK;
}

loader_runtime_t loader_runtime_get(void)
{
    UBaseType_t ux_saved;
    BaseType_t in_isr;
    loader_runtime_t copy;

    loader_runtime_enter(&ux_saved, &in_isr);
    copy = g_loader_runtime;
    loader_runtime_exit(ux_saved, in_isr);
    return copy;
}

exit_code_t loader_runtime_set(const loader_runtime_t *runtime)
{
    UBaseType_t ux_saved;
    BaseType_t in_isr;

    if (runtime == NULL) {
        return EXIT_INVALID_PARAM;
    }

    loader_runtime_enter(&ux_saved, &in_isr);
    g_loader_runtime = *runtime;
    loader_runtime_exit(ux_saved, in_isr);
    return EXIT_OK;
}

void loader_runtime_update_measurements(float current, float voltage,
                                        float power, float resistance,
                                        float temperature)
{
    UBaseType_t ux_saved;
    BaseType_t in_isr;

    loader_runtime_enter(&ux_saved, &in_isr);
    g_loader_runtime.current_measurement = current;
    g_loader_runtime.voltage_measurement = voltage;
    g_loader_runtime.power_measurement = power;
    g_loader_runtime.resistance_measurement = resistance;
    g_loader_runtime.temperature_measurement = temperature;
    loader_runtime_exit(ux_saved, in_isr);
}

void loader_runtime_set_state(loader_state_t state)
{
    UBaseType_t ux_saved;
    BaseType_t in_isr;

    loader_runtime_enter(&ux_saved, &in_isr);
    g_loader_runtime.state = state;
    loader_runtime_exit(ux_saved, in_isr);
}

void loader_runtime_enter_fault(loader_error_t error)
{
    UBaseType_t ux_saved;
    BaseType_t in_isr;

    loader_runtime_enter(&ux_saved, &in_isr);
    g_loader_runtime.error = error;
    g_loader_runtime.state = LOADER_STATE_ERROR;
    loader_runtime_exit(ux_saved, in_isr);
}

void loader_runtime_clear_fault(void)
{
    UBaseType_t ux_saved;
    BaseType_t in_isr;

    loader_runtime_enter(&ux_saved, &in_isr);
    if (g_loader_runtime.state == LOADER_STATE_ERROR) {
        g_loader_runtime.error = LOADER_ERROR_NONE;
        g_loader_runtime.state = LOADER_STATE_IDLE;
    }
    loader_runtime_exit(ux_saved, in_isr);
}

void loader_runtime_set_mode(loader_mode_t mode)
{
    UBaseType_t ux_saved;
    BaseType_t in_isr;

    loader_runtime_enter(&ux_saved, &in_isr);
    g_loader_runtime.mode = mode;
    loader_runtime_exit(ux_saved, in_isr);
}

void loader_runtime_set_current_setpoint(float value)
{
    UBaseType_t ux_saved;
    BaseType_t in_isr;

    if (value < 0.0f) {
        value = 0.0f;
    }
    if (value > LOADER_CURRENT_MAX) {
        value = LOADER_CURRENT_MAX;
    }

    loader_runtime_enter(&ux_saved, &in_isr);
    g_loader_runtime.current_setpoint = value;
    loader_runtime_exit(ux_saved, in_isr);
}

void loader_runtime_set_voltage_setpoint(float value)
{
    UBaseType_t ux_saved;
    BaseType_t in_isr;

    if (value < 0.0f) {
        value = 0.0f;
    }
    if (value > LOADER_VOLTAGE_MAX) {
        value = LOADER_VOLTAGE_MAX;
    }

    loader_runtime_enter(&ux_saved, &in_isr);
    g_loader_runtime.voltage_setpoint = value;
    loader_runtime_exit(ux_saved, in_isr);
}

void loader_runtime_set_power_setpoint(float value)
{
    UBaseType_t ux_saved;
    BaseType_t in_isr;

    if (value < 0.0f) {
        value = 0.0f;
    }
    if (value > LOADER_POWER_MAX) {
        value = LOADER_POWER_MAX;
    }

    loader_runtime_enter(&ux_saved, &in_isr);
    g_loader_runtime.power_setpoint = value;
    loader_runtime_exit(ux_saved, in_isr);
}

void loader_runtime_set_resistance_setpoint(float value)
{
    UBaseType_t ux_saved;
    BaseType_t in_isr;

    if (value < LOADER_RESISTANCE_EPSILON) {
        value = LOADER_RESISTANCE_EPSILON;
    }
    if (value > LOADER_RESISTANCE_MAX) {
        value = LOADER_RESISTANCE_MAX;
    }

    loader_runtime_enter(&ux_saved, &in_isr);
    g_loader_runtime.resistance_setpoint = value;
    loader_runtime_exit(ux_saved, in_isr);
}
