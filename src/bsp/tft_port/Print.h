#ifndef _PRINT_H_
#define _PRINT_H_

#include <stdint.h>
#include <stddef.h>
#include <string.h>

class __FlashStringHelper;

class String {
public:
  String() {}
  String(const char *s) : _s(s) {}
  unsigned int length() const { return _s ? (unsigned int)strlen(_s) : 0; }
  const char *c_str() const { return _s ? _s : ""; }
  operator const char *() const { return c_str(); }
private:
  const char *_s;
};

#define F(s) ((const __FlashStringHelper *)(s))

class Print {
public:
  virtual size_t write(uint8_t c) = 0;
  size_t write(const char *s) { return write((const uint8_t *)s, strlen(s)); }
  size_t write(const uint8_t *buf, size_t size);
  size_t write(const char *buf, size_t size) { return write((const uint8_t *)buf, size); }

  size_t print(const char []);
  size_t print(char);
  size_t print(int, int base = 10);
  size_t print(unsigned int, int base = 10);
  size_t print(long, int base = 10);
  size_t print(unsigned long, int base = 10);
  size_t print(double, int digits = 2);
  size_t print(const String &);

  size_t println(void);
  size_t println(const char[]);
  size_t println(char);
  size_t println(int, int base = 10);
  size_t println(unsigned int, int base = 10);
  size_t println(long, int base = 10);
  size_t println(unsigned long, int base = 10);
  size_t println(double, int digits = 2);
  size_t println(const String &);

private:
  size_t printNumber(unsigned long n, uint8_t base);
  size_t printFloat(double n, uint8_t digits);
  size_t printString(const char *s);
};

#endif