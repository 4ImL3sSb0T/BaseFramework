#ifndef LOADER_CONFIG_H
#define LOADER_CONFIG_H

/* 周期、限值、默认 PID、任务参数 — 见 LOADER_DESIGN.md
 *
 * 三层节奏（由 loader_task_start 拉起）：
 *   高频 TIM ISR     LOADER_CONTROL_PERIOD_S  → control_update
 *   中频 state 任务  LOADER_STATE_PERIOD_MS   → state_update + button_ticks
 *   低频 UI 任务     LOADER_UI_POLL_MS        → loader_ui_poll
 * 后两档周期定义在 loader_task.h，便于任务层统一改。
 */

/** 控制环周期 (s)，与 TIM16 配置一致，约 2 kHz */
#define LOADER_CONTROL_PERIOD_S             0.0005f

/** 满量程电流 (A)，电流环输出与 CV 外环 I_target 共用 */
#define LOADER_CURRENT_MAX                  5.0f

/** 满量程电压 / 功率 / 电阻（设定钳位，实验板） */
#define LOADER_VOLTAGE_MAX                  30.0f
#define LOADER_POWER_MAX                    50.0f
#define LOADER_RESISTANCE_MAX               1000.0f

/** 上电默认设定 */
#define LOADER_DEFAULT_CURRENT_SETPOINT     1.0f
#define LOADER_DEFAULT_VOLTAGE_SETPOINT     5.0f
#define LOADER_DEFAULT_POWER_SETPOINT       10.0f
#define LOADER_DEFAULT_RESISTANCE_SETPOINT  10.0f

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
