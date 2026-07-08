#ifndef _BUFFERED_DISPLAY_H_
#define _BUFFERED_DISPLAY_H_

#include "Adafruit_ST7735.h"
#include <Adafruit_GFX.h>

/**
 * @brief ST7735 frame-buffered display wrapper.
 *
 * Inherits from GFXcanvas16, so all draw calls render off-screen first.
 * Call flush() to push the entire framebuffer to the physical ST7735.
 *
 * Memory: 128 x 160 x 2 = 40,960 bytes (dynamically allocated by GFXcanvas16).
 */
class BufferedDisplay : public GFXcanvas16 {
public:
  BufferedDisplay(Adafruit_ST7735 *tft)
    : GFXcanvas16(128, 160, true), _tft(tft) {}

  /// @brief Push the entire framebuffer to the physical ST7735 display.
  void flush() {
    _tft->startWrite();
    _tft->setAddrWindow(0, 0, 128, 160);
    _tft->writePixels(buffer, 128 * 160);
    _tft->endWrite();
  }

  /// @brief Get the underlying ST7735 driver (for init, setRotation, etc.).
  Adafruit_ST7735 *tft() { return _tft; }

private:
  Adafruit_ST7735 *_tft;
};

#endif
