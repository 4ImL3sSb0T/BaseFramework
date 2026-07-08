/*
 * spi_flash.c — SPI Flash BSP implementation
 *
 * Uses HAL_SPI_TransmitReceive for continuous clock — required by SPI NOR Flash.
 * PB12=CS, PB13=SCK, PB14=MISO, PB15=MOSI.
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

/* ── Core transaction ────────────────────────────────────────────────── */

/**
 * SPI Flash transaction — clock never stops during CS-low.
 *
 * HAL_SPI_TransmitReceive is full-duplex: for every byte shifted out on MOSI,
 * one byte is simultaneously shifted in from MISO.  To implement a write-then-read
 * sequence with continuous clock, we build a combined TX buffer:
 *
 *   [ write_buf (N bytes) ] [ 0xFF × read_size (clocks for read) ]
 *
 * The RX side receives N bytes of don't-care garbage, then M bytes of real data.
 */
int spi_flash_write_read(const uint8_t *write_buf, size_t write_size,
                         uint8_t *read_buf, size_t read_size) {
    int rc = 0;

    /* ── Pure write (no read phase) ────────────────────────────────── */
    if (read_size == 0) {
        spi_flash_cs_low();
        rc = (HAL_SPI_Transmit(&hspi2, (uint8_t *)write_buf, write_size,
                               HAL_MAX_DELAY) == HAL_OK) ? 0 : -1;
        spi_flash_cs_high();
        return rc;
    }

    /* ── Write + Read — clock must be continuous ───────────────────── */
    uint8_t tx[write_size + read_size];
    uint8_t rx[write_size + read_size];

    /* copy command + address phase */
    for (size_t i = 0; i < write_size; i++) {
        tx[i] = write_buf[i];
    }
    /* dummy bytes after command to generate clocks for read phase */
    for (size_t i = 0; i < read_size; i++) {
        tx[write_size + i] = 0xFF;
    }

    spi_flash_cs_low();
    HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(&hspi2, tx, rx,
                                                    write_size + read_size,
                                                    HAL_MAX_DELAY);
    spi_flash_cs_high();

    if (st != HAL_OK) return -1;

    /* extract read data from the tail of the RX buffer */
    for (size_t i = 0; i < read_size; i++) {
        read_buf[i] = rx[write_size + i];
    }

    return 0;
}

/* ── Init ────────────────────────────────────────────────────────────── */

void spi_flash_init(void) {
    spi_flash_cs_high();   /* idle high */
}

/* ── JEDEC ID read ──────────────────────────────────────────────────── */

int spi_flash_read_jedec_id(uint8_t *mf_id, uint8_t *type_id, uint8_t *capacity_id) {
    uint8_t cmd = 0x9F;
    uint8_t id[3];
    int rc = spi_flash_write_read(&cmd, 1, id, 3);
    if (rc == 0) {
        *mf_id      = id[0];
        *type_id    = id[1];
        *capacity_id = id[2];
    }
    return rc;
}
