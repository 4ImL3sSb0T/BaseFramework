/*
 * spi_flash.h — SPI Flash BSP layer
 *
 * Hardware abstraction for SPI2 Flash on STM32H750.
 * PB12=CS, PB13=SCK, PB14=MISO, PB15=MOSI.
 */

#ifndef _BSP_SPI_FLASH_H_
#define _BSP_SPI_FLASH_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void spi_flash_cs_low(void);
void spi_flash_cs_high(void);

/**
 * SPI Flash transaction — atomic CS-low → write cmd → read data → CS-high.
 * Clock is continuous throughout (via HAL_SPI_TransmitReceive),
 * which is required by all SPI NOR Flash chips.
 */
int  spi_flash_write_read(const uint8_t *write_buf, size_t write_size,
                          uint8_t *read_buf, size_t read_size);

void spi_flash_init(void);

/**
 * Read JEDEC ID (command 0x9F). Returns 3 bytes: mf_id, type_id, capacity_id.
 * On success returns 0, on failure returns -1.
 */
int  spi_flash_read_jedec_id(uint8_t *mf_id, uint8_t *type_id, uint8_t *capacity_id);

#ifdef __cplusplus
}
#endif

#endif /* _BSP_SPI_FLASH_H_ */
