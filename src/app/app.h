#ifndef APP_H
#define APP_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 产品入口：板级设施 + 电子负载任务。
 * 须在 FreeRTOS 调度器已运行后调用（如 defaultTask）。
 */
void app_start(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_H */
