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

int  spi_flash_transmit(const uint8_t *data, size_t size);
int  spi_flash_receive(uint8_t *data, size_t size);

/** Atomic CS-low → write → read → CS-high transaction. */
int  spi_flash_write_read(const uint8_t *write_buf, size_t write_size,
                          uint8_t *read_buf, size_t read_size);

void spi_flash_init(void);

#ifdef __cplusplus
}
#endif

#endif /* _BSP_SPI_FLASH_H_ */
