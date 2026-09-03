#ifndef _ARDUINO_H_
#define _ARDUINO_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ARDUINO 100
#define F_CPU 480000000UL

#define PROGMEM
#define PSTR(s) (s)

#define pgm_read_byte(addr)   (*(const uint8_t  *)(addr))
#define pgm_read_word(addr)   (*(const uint16_t *)(addr))
#define pgm_read_dword(addr)  (*(const uint32_t *)(addr))
#define pgm_read_ptr(addr)    (*(void * const *)(addr))

#ifndef high
#define HIGH 1
#endif
#ifndef low
#define LOW 0
#endif

#define OUTPUT 1
#define INPUT 0
#define INPUT_PULLUP 2

#define MSBFIRST 1
#define LSBFIRST 0

#define SPI_MODE0 0
#define SPI_MODE1 1
#define SPI_MODE2 2
#define SPI_MODE3 3

typedef bool boolean;

void delay(uint32_t ms);
void delayMicroseconds(uint32_t us);
void pinMode(int8_t pin, uint8_t mode);
void digitalWrite(int8_t pin, uint8_t val);
int digitalRead(int8_t pin);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#define PI 3.1415926535897932384626433832795f

template <typename T> static inline T radians(T deg) { return deg * (T)(PI / 180.0); }
template <typename T> static inline T degrees(T rad) { return rad * (T)(180.0 / PI); }

template <typename T> static inline T min(T a, T b) { return (a < b) ? a : b; }
template <typename T> static inline T max(T a, T b) { return (a > b) ? a : b; }
template <typename T> static inline T abs(T x) { return (x < 0) ? -x : x; }
template <typename T> static inline T constrain(T x, T a, T b) { return (x < a) ? a : ((x > b) ? b : x); }
template <typename T> static inline T map(T x, T in_min, T in_max, T out_min, T out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
#endif

#endif