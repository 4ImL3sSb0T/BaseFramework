/**
 * @file tft_fb.cpp
 * @brief ST7735 frame buffer: draw offline, flush manually over SPI.
 */
#include "tft_fb.h"
#include "tft_port.h"
#include "SpiWrapper.h"
#include "Arduino.h"

#include <new>
#include <string.h>

/* Physical panel max is 128×160; covers all rotations. */
static constexpr uint32_t TFT_FB_MAX_PIXELS =
    (uint32_t)ST7735_TFTWIDTH_128 * (uint32_t)ST7735_TFTHEIGHT_160;

static Adafruit_ST7735 s_tft(&SPI, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);
static uint16_t s_pixels[TFT_FB_MAX_PIXELS];
static uint8_t s_canvas_storage[sizeof(GFXcanvas16)];
static GFXcanvas16 *s_canvas = nullptr;
static int16_t s_fb_w = 0;
static int16_t s_fb_h = 0;
static bool s_ready = false;

/** Canvas with externally provided static pixel memory (no malloc). */
class ExternalCanvas16 : public GFXcanvas16 {
public:
  ExternalCanvas16(uint16_t w, uint16_t h, uint16_t *ext)
      : GFXcanvas16(w, h, false) {
    buffer = ext;
    buffer_owned = false;
  }
};

/**
 * Send @p count RGB565 pixels (host little-endian) as big-endian over SPI.
 * Does not touch CS/DC; caller must hold an open write transaction.
 */
static void push_pixels_be(const uint16_t *src, uint32_t count) {
  /* Chunked line staging: correct endian without mutating the frame buffer. */
  uint16_t tmp[64];
  while (count > 0) {
    uint32_t n = (count > 64u) ? 64u : count;
    for (uint32_t i = 0; i < n; i++) {
      tmp[i] = __builtin_bswap16(src[i]);
    }
    SPI.writeBytes(reinterpret_cast<const uint8_t *>(tmp), n * 2u);
    src += n;
    count -= n;
  }
}

bool tft_fb_init(uint8_t rotation, uint8_t tab) {
  s_tft.initR(tab);
  s_tft.setRotation(rotation & 3u);

  const int16_t w = s_tft.width();
  const int16_t h = s_tft.height();
  if (w <= 0 || h <= 0 ||
      (uint32_t)w * (uint32_t)h > TFT_FB_MAX_PIXELS) {
    s_ready = false;
    return false;
  }

  if (s_canvas) {
    s_canvas->~GFXcanvas16();
    s_canvas = nullptr;
  }

  s_fb_w = w;
  s_fb_h = h;
  s_canvas = new (s_canvas_storage) ExternalCanvas16(
      (uint16_t)w, (uint16_t)h, s_pixels);
  s_canvas->fillScreen(ST77XX_BLACK);

  /* Clear the panel once so residual content is gone before first flush. */
  s_tft.fillScreen(ST77XX_BLACK);

  s_ready = true;
  return true;
}

Adafruit_GFX &tft_fb(void) {
  return *s_canvas;
}

Adafruit_ST7735 &tft_fb_hw(void) {
  return s_tft;
}

uint16_t *tft_fb_buffer(void) {
  return s_pixels;
}

int16_t tft_fb_width(void) {
  return s_fb_w;
}

int16_t tft_fb_height(void) {
  return s_fb_h;
}

void tft_fb_flush_rect(int16_t x, int16_t y, int16_t w, int16_t h) {
  if (!s_ready || !s_canvas || w <= 0 || h <= 0) {
    return;
  }

  /* Clip to buffer bounds. */
  if (x >= s_fb_w || y >= s_fb_h) {
    return;
  }
  if (x < 0) {
    w += x;
    x = 0;
  }
  if (y < 0) {
    h += y;
    y = 0;
  }
  if (x + w > s_fb_w) {
    w = s_fb_w - x;
  }
  if (y + h > s_fb_h) {
    h = s_fb_h - y;
  }
  if (w <= 0 || h <= 0) {
    return;
  }

  const int16_t stride = s_fb_w;
  uint16_t *buf = s_pixels;

  s_tft.startWrite();
  s_tft.setAddrWindow((uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h);

  if (x == 0 && w == stride) {
    /* Full-width block is contiguous in memory. */
    push_pixels_be(buf + (uint32_t)y * (uint32_t)stride,
                   (uint32_t)w * (uint32_t)h);
  } else {
    for (int16_t row = 0; row < h; row++) {
      push_pixels_be(buf + (uint32_t)(y + row) * (uint32_t)stride + (uint32_t)x,
                     (uint32_t)w);
    }
  }

  s_tft.endWrite();
}

void tft_fb_flush(void) {
  if (!s_ready) {
    return;
  }
  tft_fb_flush_rect(0, 0, s_fb_w, s_fb_h);
}
