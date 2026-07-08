#include "Arduino.h"
#include "main.h"

void delay(uint32_t ms) {
  HAL_Delay(ms);
}

void delayMicroseconds(uint32_t us) {
  uint32_t start = DWT->CYCCNT;
  uint32_t cycles = us * (SystemCoreClock / 1000000U);
  while ((DWT->CYCCNT - start) < cycles) { __NOP(); }
}

void pinMode(int8_t pin, uint8_t mode) {
  (void)pin; (void)mode;
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