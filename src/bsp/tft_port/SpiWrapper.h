#ifndef _SPI_H_
#define _SPI_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus

class SPISettings {
public:
  SPISettings(uint32_t freq = 0, uint8_t bitOrder = 1, uint8_t dataMode = 0)
    : _freq(freq), _bitOrder(bitOrder), _dataMode(dataMode) {}
  uint32_t _freq;
  uint8_t _bitOrder;
  uint8_t _dataMode;
};

class SPIClass {
public:
  SPIClass();

  void begin();
  void end();
  void usingInterrupt(int) {}
  void notUsingInterrupt(int) {}

  void beginTransaction(SPISettings settings);
  void endTransaction();

  uint8_t transfer(uint8_t b);
  uint8_t transfer(uint8_t *buf, size_t len);
  uint16_t transfer16(uint16_t w);

  void write(uint8_t b);
  void write16(uint16_t w);
  void writeBytes(const uint8_t *data, size_t size);
  void writePixels(const uint16_t *colors, size_t len);

  void setClockDivider(uint8_t div);
  void setBitOrder(uint8_t order);
  void setDataMode(uint8_t mode);
  void setFrequency(uint32_t freq);
};

extern SPIClass SPI;

#endif /* __cplusplus */

#endif