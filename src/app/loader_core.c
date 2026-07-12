#include "loader_core.h"
#include "bsp/tim/bsp_timer.h"
#include "service/load/load_out.h"

pid_controller_t pid_current = {
    .kp = 0.1f,
    .ki = 0.01f,
    .kd = 0.005f,
    .error_prev = 0.0f,
    .integral = 0.0f,
    .output = 0.0f,
    .output_max = 12.0f, // Example max output voltage
    .output_min = 0.0f,  // Example min output voltage
    .integral_max = 10.0f, // Prevent integral windup
    .integral_min = -10.0f, // Prevent integral windup
    .dt = 0.01f // Example sampling time (100 Hz)
};

exit_code_t loader_core_init(void) {
    loader_runtime_init();
    sense_init(SENSE_MODE_ADC);
    load_out_init();
    bsp_timer_init();
    bsp_timer_register_callback(loader_core_control_update);
    pid_init(&pid_current, 0.1f, 0.01f, 0.005f, 0.01f, 10.0f, -10.0f); // Initialize PID with example parameters
    return EXIT_OK;
}

void loader_core_control_update(void) {
    float current = sense_get_current();
    float voltage = sense_get_voltage();

    loader_runtime_t runtime = loader_runtime_get();
    runtime.current_measurement = current;
    runtime.voltage_measurement = voltage;
    runtime.power_measurement = current * voltage; // Calculate power
    loader_runtime_set(&runtime);

    /* 控制环写执行器入口：RUN 且允许输出时 load_out_set(out_norm)，否则关断 */
    /* load_out_set(pid_current.output); */
}
