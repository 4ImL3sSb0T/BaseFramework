#include "Print.h"
#include <string.h>

size_t Print::write(const uint8_t *buf, size_t size) {
  size_t n = 0;
  while (size--) n += write(*buf++);
  return n;
}

size_t Print::printString(const char *s) {
  size_t n = 0;
  while (*s) { n += write((uint8_t)*s++); }
  return n;
}

size_t Print::printNumber(unsigned long n, uint8_t base) {
  char buf[8 * sizeof(long) + 1];
  char *str = &buf[sizeof(buf) - 1];
  *str = '\0';
  if (base < 2) base = 10;
  if (n == 0) {
    *--str = '0';
  } else {
    while (n) {
      unsigned char d = (unsigned char)(n % base);
      *--str = (char)(d < 10 ? '0' + d : 'A' + d - 10);
      n /= base;
    }
  }
  return printString(str);
}

size_t Print::printFloat(double n, uint8_t digits) {
  size_t r = 0;
  if (n < 0) { r += write((uint8_t)'-'); n = -n; }
  unsigned long whole = (unsigned long)n;
  r += printNumber(whole, 10);
  r += write((uint8_t)'.');
  double frac = n - (double)whole;
  for (uint8_t i = 0; i < digits; i++) {
    frac *= 10.0;
    unsigned char d = (unsigned char)frac;
    r += write((uint8_t)('0' + d));
    frac -= d;
  }
  return r;
}

size_t Print::print(const char s[]) { return printString(s); }
size_t Print::print(char c) { return write((uint8_t)c); }
size_t Print::print(int n, int base) {
  if (n < 0 && base == 10) { size_t r = write((uint8_t)'-'); return r + printNumber((unsigned long)(-n), (uint8_t)base); }
  return printNumber((unsigned long)n, (uint8_t)base);
}
size_t Print::print(unsigned int n, int base) { return printNumber((unsigned long)n, (uint8_t)base); }
size_t Print::print(long n, int base) {
  if (n < 0 && base == 10) { size_t r = write((uint8_t)'-'); return r + printNumber((unsigned long)(-n), (uint8_t)base); }
  return printNumber((unsigned long)n, (uint8_t)base);
}
size_t Print::print(unsigned long n, int base) { return printNumber(n, (uint8_t)base); }
size_t Print::print(double d, int digits) { return printFloat(d, (uint8_t)digits); }
size_t Print::print(const String &s) { return printString(s.c_str()); }

size_t Print::println(void) { return write("\r\n"); }
size_t Print::println(const char s[]) { return printString(s) + write("\r\n"); }
size_t Print::println(char c) { return write((uint8_t)c) + write("\r\n"); }
size_t Print::println(int n, int base) { return print(n, base) + write("\r\n"); }
size_t Print::println(unsigned int n, int base) { return print(n, base) + write("\r\n"); }
size_t Print::println(long n, int base) { return print(n, base) + write("\r\n"); }
size_t Print::println(unsigned long n, int base) { return print(n, base) + write("\r\n"); }
size_t Print::println(double d, int digits) { return print(d, digits) + write("\r\n"); }
size_t Print::println(const String &s) { return printString(s.c_str()) + write("\r\n"); }