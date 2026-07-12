#ifndef LOADER_CONFIG_H
#define LOADER_CONFIG_H

/* 周期、限值、默认 PID、任务参数 — 见 LOADER_DESIGN.md */
#define LOADER_CONTROL_PERIOD_MS 0.0005 // 2 kHz control loop
#define LOADER_OVERCURRENT_LIMIT 5.0f

#define LOADER_OVERTEMPERATURE_LIMIT 50.0f // Example temperature limit in Celsius
#define LOADER_OVERCURRENT_LIMIT 5.0f // Example current limit in Amperes

#define LOADER_DEFAULT_PID_CURRENT_KP 1.0f
#define LOADER_DEFAULT_PID_CURRENT_KI 0.01f
#define LOADER_DEFAULT_PID_CURRENT_KD 0.0f
#define LOADER_DEFAULT_PID_CURRENT_OUTPUT_MAX 3.3f // Example max output voltage for current control
#define LOADER_DEFAULT_PID_CURRENT_OUTPUT_MIN 0.0f // Example min output voltage for current control
#define LOADER_DEFAULT_PID_CURRENT_INTEGRAL_MAX 10.0f // Prevent integral windup
#define LOADER_DEFAULT_PID_CURRENT_INTEGRAL_MIN -10.0f // Prevent integral windup
#define LOADER_DEFAULT_PID_CURRENT_DT 0.0005f // Example sampling time (2000 Hz)

#define LOADER_DEFAULT_PID_VOLTAGE_KP 0.1f
#define LOADER_DEFAULT_PID_VOLTAGE_KI 0.01f
#define LOADER_DEFAULT_PID_VOLTAGE_KD 0.005f
#define LOADER_DEFAULT_PID_VOLTAGE_OUTPUT_MAX 3.3f // Example max output voltage for voltage control
#define LOADER_DEFAULT_PID_VOLTAGE_OUTPUT_MIN 0.0f // Example min output voltage for voltage control
#define LOADER_DEFAULT_PID_VOLTAGE_INTEGRAL_MAX 10.0f // Prevent integral windup
#define LOADER_DEFAULT_PID_VOLTAGE_INTEGRAL_MIN -10.0f // Prevent integral windup
#define LOADER_DEFAULT_PID_VOLTAGE_DT 0.01f // Example sampling time (100 Hz)


#endif /* LOADER_CONFIG_H */
