/*
 * spi_flash.c — SPI Flash BSP implementation
 */

#include "spi_flash.h"
#include "spi.h"
#include "main.h"

/* ── CS control ─────────────────────────────────────────────────────── */

void spi_flash_cs_low(void) {
    HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_RESET);
}

void spi_flash_cs_high(void) {
    HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_SET);
}

/* ── SPI IO ──────────────────────────────────────────────────────────── */

int spi_flash_transmit(const uint8_t *data, size_t size) {
    return (HAL_SPI_Transmit(&hspi2, (uint8_t *)data, size, HAL_MAX_DELAY) == HAL_OK) ? 0 : -1;
}

int spi_flash_receive(uint8_t *data, size_t size) {
    return (HAL_SPI_Receive(&hspi2, data, size, HAL_MAX_DELAY) == HAL_OK) ? 0 : -1;
}

int spi_flash_write_read(const uint8_t *write_buf, size_t write_size,
                         uint8_t *read_buf, size_t read_size) {
    spi_flash_cs_low();

    if (write_size > 0) {
        if (spi_flash_transmit(write_buf, write_size) != 0) {
            spi_flash_cs_high();
            return -1;
        }
    }

    if (read_size > 0) {
        if (spi_flash_receive(read_buf, read_size) != 0) {
            spi_flash_cs_high();
            return -2;
        }
    }

    spi_flash_cs_high();
    return 0;
}

/* ── Init ────────────────────────────────────────────────────────────── */

void spi_flash_init(void) {
    spi_flash_cs_high();   /* idle high */
}
