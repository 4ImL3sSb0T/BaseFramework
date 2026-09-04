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

void     spi_flash_cs_low(void);
void     spi_flash_cs_high(void);
uint8_t  spi_flash_read_write_byte(uint8_t tx_data);

int      spi_flash_write_read(const uint8_t *write_buf, size_t write_size,
                              uint8_t *read_buf, size_t read_size);

void     spi_flash_init(void);

uint16_t spi_flash_read_id(void);
int      spi_flash_read_jedec_id(uint8_t *mf_id, uint8_t *type_id, uint8_t *capacity_id);

#ifdef __cplusplus
}
#endif

#endif /* _BSP_SPI_FLASH_H_ */
