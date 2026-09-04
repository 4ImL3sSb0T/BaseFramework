#ifndef BOARD_H
#define BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Cube 外设 init 之后、RTOS 启动之前：
 * SPI Flash 分频与 dummy 字节（必须在调度器前完成）。
 */
void board_early_init(void);

/**
 * 板级设施：日志、串口 shell、文件系统。
 * 须在 FreeRTOS 调度器已运行后调用。
 */
void board_init(void);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_H */
