/**
 * @file SpiWrapper.cpp
 * @brief SPI1 bridge for Adafruit GFX / ST7735.
 *
 * Small transfers (commands, single pixels): blocking HAL_SPI_Transmit.
 * Large transfers (frame buffer push): SPI1 TX DMA + wait for complete.
 */
#include "SpiWrapper.h"
#include "Arduino.h"

#include "cmsis_os2.h"

extern SPI_HandleTypeDef hspi1;

SPIClass SPI;

/** Below this size DMA setup costs more than polling. */
#ifndef SPI_DMA_MIN_BYTES
#define SPI_DMA_MIN_BYTES 64u
#endif

static volatile uint8_t s_spi1_tx_done;
static volatile uint8_t s_spi1_tx_error;

extern "C" void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi != NULL && hspi->Instance == SPI1) {
    s_spi1_tx_done = 1u;
  }
}

extern "C" void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi != NULL && hspi->Instance == SPI1) {
    s_spi1_tx_error = 1u;
    s_spi1_tx_done = 1u;
  }
}

static void spi1_wait_ready(void)
{
  uint32_t guard = 0u;
  while (HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY) {
    if ((++guard & 0xFFu) == 0u) {
      /* Yield so other tasks run while a previous xfer finishes */
      if (osKernelGetState() == osKernelRunning) {
        osDelay(1);
      }
    }
    if (guard > 1000000u) {
      (void)HAL_SPI_Abort(&hspi1);
      break;
    }
  }
}

static void spi1_transmit_blocking(uint8_t *data, uint16_t size)
{
  spi1_wait_ready();
  (void)HAL_SPI_Transmit(&hspi1, data, size, HAL_MAX_DELAY);
}

static void spi1_transmit_dma(uint8_t *data, uint16_t size)
{
  if (data == NULL || size == 0u) {
    return;
  }

  /* No DMA handle → fall back */
  if (hspi1.hdmatx == NULL) {
    spi1_transmit_blocking(data, size);
    return;
  }

  spi1_wait_ready();

  s_spi1_tx_done = 0u;
  s_spi1_tx_error = 0u;

  if (HAL_SPI_Transmit_DMA(&hspi1, data, size) != HAL_OK) {
    spi1_transmit_blocking(data, size);
    return;
  }

  /* Wait for IRQ completion; yield under FreeRTOS */
  uint32_t guard = 0u;
  while (s_spi1_tx_done == 0u) {
    if (HAL_SPI_GetState(&hspi1) == HAL_SPI_STATE_ERROR) {
      s_spi1_tx_error = 1u;
      break;
    }
    if (osKernelGetState() == osKernelRunning) {
      osDelay(1);
    } else {
      /* Pre-scheduler: tight poll */
    }
    if (++guard > 5000u) { /* ~5s worst case @ 1ms delay */
      (void)HAL_SPI_Abort(&hspi1);
      s_spi1_tx_error = 1u;
      break;
    }
  }

  if (s_spi1_tx_error != 0u) {
    /* Best-effort recovery for next transfer */
    (void)HAL_SPI_Abort(&hspi1);
  }
}

SPIClass::SPIClass() {}

void SPIClass::begin() {}

void SPIClass::end() {}

void SPIClass::beginTransaction(SPISettings) {}
void SPIClass::endTransaction() {}

uint8_t SPIClass::transfer(uint8_t b)
{
  spi1_transmit_blocking(&b, 1);
  return b;
}

uint8_t SPIClass::transfer(uint8_t *buf, size_t len)
{
  if (buf == NULL || len == 0u) {
    return 0;
  }
  if (len > 0xFFFFu) {
    len = 0xFFFFu;
  }
  if (len >= SPI_DMA_MIN_BYTES) {
    spi1_transmit_dma(buf, (uint16_t)len);
  } else {
    spi1_transmit_blocking(buf, (uint16_t)len);
  }
  return 0;
}

uint16_t SPIClass::transfer16(uint16_t w)
{
  uint8_t buf[2] = {(uint8_t)(w >> 8), (uint8_t)(w & 0xFF)};
  spi1_transmit_blocking(buf, 2);
  return w;
}

void SPIClass::write(uint8_t b)
{
  spi1_transmit_blocking(&b, 1);
}

void SPIClass::write16(uint16_t w)
{
  uint8_t buf[2] = {(uint8_t)(w >> 8), (uint8_t)(w & 0xFF)};
  spi1_transmit_blocking(buf, 2);
}

void SPIClass::writeBytes(const uint8_t *data, size_t size)
{
  if (data == NULL || size == 0u) {
    return;
  }
  if (size > 0xFFFFu) {
    size = 0xFFFFu;
  }
  if (size >= SPI_DMA_MIN_BYTES) {
    spi1_transmit_dma(const_cast<uint8_t *>(data), (uint16_t)size);
  } else {
    spi1_transmit_blocking(const_cast<uint8_t *>(data), (uint16_t)size);
  }
}

void SPIClass::writePixels(const uint16_t *colors, size_t len)
{
  /* Caller must already match panel byte order; length in pixels. */
  if (colors == NULL || len == 0u) {
    return;
  }
  size_t bytes = len * 2u;
  if (bytes > 0xFFFFu) {
    bytes = 0xFFFFu;
  }
  writeBytes(reinterpret_cast<const uint8_t *>(colors), bytes);
}

void SPIClass::setClockDivider(uint8_t) {}
void SPIClass::setBitOrder(uint8_t) {}
void SPIClass::setDataMode(uint8_t) {}
void SPIClass::setFrequency(uint32_t) {}
