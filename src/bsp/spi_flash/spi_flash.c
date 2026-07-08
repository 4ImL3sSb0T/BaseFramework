/*
 * spi_flash.c — SPI Flash BSP implementation
 *
 * Follows STM32H743 reference project pattern exactly:
 *   - Per-byte HAL_SPI_TransmitReceive with manual CS control
 *   - Read ID uses command 0x90 (Manufacturer/Device ID)
 *
 * PB12=CS, PB13=SCK, PB14=MISO, PB15=MOSI.
 */

#include "spi_flash.h"
#include "spi.h"
#include "main.h"

#define SPI_TIMEOUT 1000

void spi_flash_cs_low(void) {
    HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_RESET);
}

void spi_flash_cs_high(void) {
    HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_SET);
}

uint8_t spi_flash_read_write_byte(uint8_t tx_data) {
    uint8_t rx_data;
    HAL_SPI_TransmitReceive(&hspi2, &tx_data, &rx_data, 1, SPI_TIMEOUT);
    return rx_data;
}

int spi_flash_write_read(const uint8_t *write_buf, size_t write_size,
                         uint8_t *read_buf, size_t read_size) {
    spi_flash_cs_low();

    for (size_t i = 0; i < write_size; i++) {
        spi_flash_read_write_byte(write_buf[i]);
    }

    for (size_t i = 0; i < read_size; i++) {
        read_buf[i] = spi_flash_read_write_byte(0xFF);
    }

    spi_flash_cs_high();
    return 0;
}

void spi_flash_init(void) {
    spi_flash_cs_high();
}

/* ── Read Manufacturer/Device ID (command 0x90, matches reference) ─── */

uint16_t spi_flash_read_id(void) {
    uint16_t id = 0;

    spi_flash_cs_low();
    spi_flash_read_write_byte(0x90);
    spi_flash_read_write_byte(0x00);
    spi_flash_read_write_byte(0x00);
    spi_flash_read_write_byte(0x00);
    id |= (uint16_t)spi_flash_read_write_byte(0xFF) << 8;
    id |= (uint16_t)spi_flash_read_write_byte(0xFF);
    spi_flash_cs_high();

    return id;
}

/* ── JEDEC ID read (command 0x9F, for SFUD) ──────────────────────── */

int spi_flash_read_jedec_id(uint8_t *mf_id, uint8_t *type_id, uint8_t *capacity_id) {
    uint8_t cmd = 0x9F;
    uint8_t id[3];
    int rc = spi_flash_write_read(&cmd, 1, id, 3);
    if (rc == 0) {
        *mf_id       = id[0];
        *type_id     = id[1];
        *capacity_id = id[2];
    }
    return rc;
}
