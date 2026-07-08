/*
 * spi_flash.c — SPI Flash BSP implementation
 *
 * Uses HAL_SPI_TransmitReceive for continuous clock — required by SPI NOR Flash.
 * Bulk transfer approach restores efficiency after fixing SPI clock speed.
 * PB12=CS, PB13=SCK, PB14=MISO, PB15=MOSI.
 */

#include "spi_flash.h"
#include "spi.h"
#include "main.h"

#define SPI_TIMEOUT 1000

/* ── CS control ─────────────────────────────────────────────────────── */

void spi_flash_cs_low(void) {
    HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_RESET);
}

void spi_flash_cs_high(void) {
    HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_SET);
}

/* ── Single byte (for init dummy) ───────────────────────────────────── */

uint8_t spi_flash_read_write_byte(uint8_t tx_data) {
    uint8_t rx_data;
    HAL_SPI_TransmitReceive(&hspi2, &tx_data, &rx_data, 1, SPI_TIMEOUT);
    return rx_data;
}

/* ── Core transaction (bulk, continuous clock) ───────────────────────── */

int spi_flash_write_read(const uint8_t *write_buf, size_t write_size,
                         uint8_t *read_buf, size_t read_size) {
    if (read_size == 0) {
        spi_flash_cs_low();
        HAL_StatusTypeDef st = HAL_SPI_Transmit(&hspi2, (uint8_t *)write_buf,
                                                 write_size, SPI_TIMEOUT);
        spi_flash_cs_high();
        return (st == HAL_OK) ? 0 : -1;
    }

    uint8_t tx[write_size + read_size];
    uint8_t rx[write_size + read_size];

    for (size_t i = 0; i < write_size; i++) {
        tx[i] = write_buf[i];
    }
    for (size_t i = 0; i < read_size; i++) {
        tx[write_size + i] = 0xFF;
    }

    spi_flash_cs_low();
    HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(&hspi2, tx, rx,
                                                    write_size + read_size,
                                                    SPI_TIMEOUT);
    spi_flash_cs_high();

    if (st != HAL_OK) return -1;

    for (size_t i = 0; i < read_size; i++) {
        read_buf[i] = rx[write_size + i];
    }
    return 0;
}

/* ── Init ────────────────────────────────────────────────────────────── */

void spi_flash_init(void) {
    spi_flash_cs_high();
}

/* ── Read Manufacturer/Device ID (command 0x90) ─────────────────────── */

uint16_t spi_flash_read_id(void) {
    uint8_t cmd[4] = {0x90, 0x00, 0x00, 0x00};
    uint8_t id[2];
    spi_flash_write_read(cmd, 4, id, 2);
    return ((uint16_t)id[0] << 8) | id[1];
}

/* ── Read JEDEC ID (command 0x9F, for SFUD) ─────────────────────────── */

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
