#ifndef LOADER_TASK_H
#define LOADER_TASK_H

/* 产品入口 / 三层节奏调度 — 见 LOADER_DESIGN.md / LOADER_UI.md
 *
 *  ┌─────────────────────────────────────────────────────────┐
 *  │  高频  TIM16 ISR     ~2 kHz   loader_core_control_update │
 *  │  中频  loaderState   ~5 ms    保护/状态 + button_ticks    │
 *  │  低频  loaderUi      ~67 ms   显示 + 按键语义 + 设定      │
 *  └─────────────────────────────────────────────────────────┘
 *
 * 入口：loader_task_start() — 初始化 core（开定时器）并创建中/低频任务
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 中频状态任务周期 (ms)
 * OCP/OTP 巡检、按键状态机 tick（与 multi_button TICKS_INTERVAL=5ms 对齐）
 */
#ifndef LOADER_STATE_PERIOD_MS
#define LOADER_STATE_PERIOD_MS  5u
#endif

/**
 * 低频 UI 任务周期 (ms)。15 fps → 1000/15 ≈ 67ms。
 * 全屏 SPI 刷新略低于上限，留余量。
 */
#ifndef LOADER_UI_POLL_MS
#define LOADER_UI_POLL_MS  67u
#endif

/**
 * 启动电子负载调度：
 * 1) loader_core_init — sense/load_out/fan + TIM 高频控制环
 * 2) 创建中频状态任务
 * 3) 创建低频 UI 任务
 *
 * 须在 FreeRTOS 调度器已运行后调用（如 defaultTask 内）。
 */
void loader_task_start(void);

/** 显示初始化（帧缓冲 + 按键 + 首帧）；由 UI 任务内部调用 */
void loader_ui_init(void);

/** UI 周期刷新：处理按键队列、读 runtime、绘制 present */
void loader_ui_poll(void);

#ifdef __cplusplus
}
#endif

#endif /* LOADER_TASK_H */
