/*
 * sys_time.c — system time for STM32H750 (Cortex-M7)
 *
 * ms timebase: HAL tick (TIM17, 1 ms)
 * cycle counter: DWT CYCCNT
 */

#include "sys_time.h"
#include "stm32h7xx.h"
#include "stm32h7xx_hal.h"

uint32_t sys_time_get_ms(void)
{
    return HAL_GetTick();
}

void dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}
