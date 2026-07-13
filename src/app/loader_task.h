#ifndef LOADER_TASK_H
#define LOADER_TASK_H

/* 产品入口 / 任务；ui、cli 声明可放这里 — 见 LOADER_DESIGN.md / LOADER_UI.md */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * UI 任务周期 (ms)。15 fps → 1000/15 ≈ 67ms。
 * 全屏 SPI@6MHz 理论上限约 18fps，15fps 略低于上限，留一点余量。
 */
#ifndef LOADER_UI_POLL_MS
#define LOADER_UI_POLL_MS  67u
#endif

/** 创建 UI 等应用任务（在 FreeRTOS 已启动后调用） */
void loader_task_init(void);

/** 显示初始化（帧缓冲 + 首帧）；也可由 UI 任务内部调用 */
void loader_ui_init(void);

/** UI 周期刷新：模拟数据 + 绘制 + present（周期见 LOADER_UI_POLL_MS） */
void loader_ui_poll(void);

#ifdef __cplusplus
}
#endif

#endif /* LOADER_TASK_H */
