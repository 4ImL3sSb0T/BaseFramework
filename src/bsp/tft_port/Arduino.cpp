#include "Arduino.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

/*
 * Prefer FreeRTOS delay when the scheduler is running (yields CPU).
 * Fall back to HAL_Delay only before osKernelStart() / from rare bare-metal paths.
 */
void delay(uint32_t ms) {
  if (ms == 0U) {
    return;
  }
  if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
    TickType_t ticks = pdMS_TO_TICKS(ms);
    if (ticks == 0) {
      ticks = 1; /* sub-tick ms still wait at least one tick */
    }
    vTaskDelay(ticks);
  } else {
    HAL_Delay(ms);
  }
}

void delayMicroseconds(uint32_t us) {
  if (us == 0U) {
    return;
  }
  /* Short busy-wait (DWT). Not used for multi-ms TFT init delays. */
  uint32_t start = DWT->CYCCNT;
  uint32_t cycles = us * (SystemCoreClock / 1000000U);
  while ((DWT->CYCCNT - start) < cycles) {
    __NOP();
  }
}

void pinMode(int8_t pin, uint8_t mode) {
  (void)pin;
  (void)mode;
}

void digitalWrite(int8_t pin, uint8_t val) {
  switch (pin) {
    case 0: HAL_GPIO_WritePin(TFT_CS_GPIO_Port,  TFT_CS_Pin,  (GPIO_PinState)val); break;
    case 1: HAL_GPIO_WritePin(TFT_DC_GPIO_Port,  TFT_DC_Pin,  (GPIO_PinState)val); break;
    case 2: HAL_GPIO_WritePin(TFT_RST_GPIO_Port, TFT_RST_Pin, (GPIO_PinState)val); break;
    default: break;
  }
}

int digitalRead(int8_t pin) {
  (void)pin;
  return 0;
}
