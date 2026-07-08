#include "SpiWrapper.h"
#include "Arduino.h"

extern SPI_HandleTypeDef hspi1;

SPIClass SPI;

SPIClass::SPIClass() {}

void SPIClass::begin() {}

void SPIClass::end() {}

void SPIClass::beginTransaction(SPISettings) {}
void SPIClass::endTransaction() {}

uint8_t SPIClass::transfer(uint8_t b) {
  HAL_SPI_Transmit(&hspi1, &b, 1, HAL_MAX_DELAY);
  return b;
}

uint8_t SPIClass::transfer(uint8_t *buf, size_t len) {
  HAL_SPI_Transmit(&hspi1, buf, len, HAL_MAX_DELAY);
  return 0;
}

uint16_t SPIClass::transfer16(uint16_t w) {
  uint8_t buf[2] = { (uint8_t)(w >> 8), (uint8_t)(w & 0xFF) };
  HAL_SPI_Transmit(&hspi1, buf, 2, HAL_MAX_DELAY);
  return w;
}

void SPIClass::write(uint8_t b) {
  HAL_SPI_Transmit(&hspi1, &b, 1, HAL_MAX_DELAY);
}

void SPIClass::write16(uint16_t w) {
  uint8_t buf[2] = { (uint8_t)(w >> 8), (uint8_t)(w & 0xFF) };
  HAL_SPI_Transmit(&hspi1, buf, 2, HAL_MAX_DELAY);
}

void SPIClass::writeBytes(const uint8_t *data, size_t size) {
  HAL_SPI_Transmit(&hspi1, (uint8_t *)data, size, HAL_MAX_DELAY);
}

void SPIClass::writePixels(const uint16_t *colors, size_t len) {
  HAL_SPI_Transmit(&hspi1, (uint8_t *)colors, len * 2, HAL_MAX_DELAY);
}

void SPIClass::setClockDivider(uint8_t) {}
void SPIClass::setBitOrder(uint8_t) {}
void SPIClass::setDataMode(uint8_t) {}
void SPIClass::setFrequency(uint32_t) {}