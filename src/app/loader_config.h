#ifndef LOADER_CONFIG_H
#define LOADER_CONFIG_H

/* 周期、限值、默认 PID、任务参数 — 见 LOADER_DESIGN.md */

/** 控制环周期 (s)，2 kHz */
#define LOADER_CONTROL_PERIOD_S             0.0005f

/** 满量程电流 (A)，电流环输出与 CV 外环 I_target 共用 */
#define LOADER_CURRENT_MAX                  5.0f

/** 软件过流阈值 (A) */
#define LOADER_OVERCURRENT_LIMIT            5.0f

/** 过温阈值 (°C) */
#define LOADER_OVERTEMPERATURE_LIMIT        50.0f

/** CP / 测量除法防零 */
#define LOADER_VOLTAGE_EPSILON              0.05f
#define LOADER_CURRENT_EPSILON              0.001f
#define LOADER_RESISTANCE_EPSILON           0.01f

/**
 * 软件电流内环开关
 * 0：外部运放硬件闭环，软件只下发 I_target（当前板级）
 * 1：软件电流 PID 闭环
 */
#define LOADER_USE_SOFTWARE_CURRENT_PID     0

/* ---------- 电流内环 PID（输出：设定电流 A；仅 USE_SOFTWARE_CURRENT_PID=1） ---------- */
#define LOADER_DEFAULT_PID_CURRENT_KP           1.0f
#define LOADER_DEFAULT_PID_CURRENT_KI           0.01f
#define LOADER_DEFAULT_PID_CURRENT_KD           0.0f
#define LOADER_DEFAULT_PID_CURRENT_OUTPUT_MAX   LOADER_CURRENT_MAX
#define LOADER_DEFAULT_PID_CURRENT_OUTPUT_MIN   0.0f
#define LOADER_DEFAULT_PID_CURRENT_INTEGRAL_MAX 1.0f
#define LOADER_DEFAULT_PID_CURRENT_INTEGRAL_MIN -1.0f
#define LOADER_DEFAULT_PID_CURRENT_DT           LOADER_CONTROL_PERIOD_S

/* ---------- 电压外环 PID（输出：I_target A，非电压） ---------- */
#define LOADER_DEFAULT_PID_VOLTAGE_KP           0.1f
#define LOADER_DEFAULT_PID_VOLTAGE_KI           0.01f
#define LOADER_DEFAULT_PID_VOLTAGE_KD           0.005f
#define LOADER_DEFAULT_PID_VOLTAGE_OUTPUT_MAX   LOADER_CURRENT_MAX
#define LOADER_DEFAULT_PID_VOLTAGE_OUTPUT_MIN   0.0f
#define LOADER_DEFAULT_PID_VOLTAGE_INTEGRAL_MAX 5.0f
#define LOADER_DEFAULT_PID_VOLTAGE_INTEGRAL_MIN -5.0f
/* 与控制环同频调用；若以后降采样再改 dt / 分频 */
#define LOADER_DEFAULT_PID_VOLTAGE_DT           LOADER_CONTROL_PERIOD_S

#endif /* LOADER_CONFIG_H */
