#ifndef __SYS_TIME_H
#define __SYS_TIME_H

#include <stdint.h>
#include "stm32h7xx.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Monotonic ms (same source as HAL tick / FreeRTOS 1 kHz). */
uint32_t sys_time_get_ms(void);

/** Enable Cortex-M7 DWT cycle counter. Call once after clocks are up. */
void dwt_init(void);

#define DWT_GET_CYCLES()   (DWT->CYCCNT)
#define DWT_RESET()        do { DWT->CYCCNT = 0; } while (0)
#define DWT_START()        do { DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk; } while (0)
#define DWT_STOP()         do { DWT->CTRL &= ~DWT_CTRL_CYCCNTENA_Msk; } while (0)

#ifdef __cplusplus
}
#endif

#endif /* __SYS_TIME_H */
