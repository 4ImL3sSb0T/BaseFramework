/**
 * @file tft_fb.cpp
 * @brief ST7735 frame buffer: draw offline, flush via SPI1 TX DMA.
 *
 * Framebuffer (LE RGB565 for Adafruit_GFX) lives in normal AXI SRAM.
 * A separate big-endian staging buffer in .dma_buf is filled then pushed
 * with one (or few) SPI DMA transfers — no per-chunk bswap + blocking TX.
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

/* Draw buffer: host LE RGB565 (cacheable AXI .bss is fine — CPU only). */
static uint16_t s_pixels[TFT_FB_MAX_PIXELS];

/*
 * SPI staging: panel wants big-endian RGB565 bytes.
 * Place in D2 .dma_buf (MPU non-cacheable) so DMA sees coherent data
 * without D-Cache clean. 40KB + UART/ADC DMA bufs still fit in 64KB.
 */
#define TFT_DMA_BUF __attribute__((section(".dma_buf"), aligned(32)))
static TFT_DMA_BUF uint16_t s_spi_be[TFT_FB_MAX_PIXELS];

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

/** LE → BE into s_spi_be[0..count), then one DMA writeBytes. */
static void push_pixels_be(const uint16_t *src, uint32_t count)
{
  if (src == NULL || count == 0u) {
    return;
  }
  if (count > TFT_FB_MAX_PIXELS) {
    count = TFT_FB_MAX_PIXELS;
  }

  /* Unrolled a bit: REV16 is one instruction per halfword on M7. */
  uint16_t *dst = s_spi_be;
  uint32_t i = 0u;
  for (; i + 4u <= count; i += 4u) {
    dst[i + 0u] = __REV16(src[i + 0u]);
    dst[i + 1u] = __REV16(src[i + 1u]);
    dst[i + 2u] = __REV16(src[i + 2u]);
    dst[i + 3u] = __REV16(src[i + 3u]);
  }
  for (; i < count; i++) {
    dst[i] = __REV16(src[i]);
  }

  SPI.writeBytes(reinterpret_cast<const uint8_t *>(s_spi_be), count * 2u);
}

/**
 * Pack a non-contiguous rect into s_spi_be as BE, then one DMA transfer.
 * Window must already be set to (x,y,w,h).
 */
static void push_rect_be(const uint16_t *fb, int16_t stride,
                         int16_t x, int16_t y, int16_t w, int16_t h)
{
  uint32_t n = 0u;
  for (int16_t row = 0; row < h; row++) {
    const uint16_t *src =
        fb + (uint32_t)(y + row) * (uint32_t)stride + (uint32_t)x;
    for (int16_t col = 0; col < w; col++) {
      s_spi_be[n++] = __REV16(src[col]);
    }
  }
  if (n > 0u) {
    SPI.writeBytes(reinterpret_cast<const uint8_t *>(s_spi_be), n * 2u);
  }
}

bool tft_fb_init(uint8_t rotation, uint8_t tab)
{
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

Adafruit_GFX &tft_fb(void)
{
  return *s_canvas;
}

Adafruit_ST7735 &tft_fb_hw(void)
{
  return s_tft;
}

uint16_t *tft_fb_buffer(void)
{
  return s_pixels;
}

int16_t tft_fb_width(void)
{
  return s_fb_w;
}

int16_t tft_fb_height(void)
{
  return s_fb_h;
}

void tft_fb_flush_rect(int16_t x, int16_t y, int16_t w, int16_t h)
{
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
    /* Contiguous in FB: convert block + single DMA. */
    push_pixels_be(buf + (uint32_t)y * (uint32_t)stride,
                   (uint32_t)w * (uint32_t)h);
  } else {
    /* Sub-rect: pack to staging then single DMA (avoids per-row setup). */
    push_rect_be(buf, stride, x, y, w, h);
  }

  s_tft.endWrite();
}

void tft_fb_flush(void)
{
  if (!s_ready) {
    return;
  }
  tft_fb_flush_rect(0, 0, s_fb_w, s_fb_h);
}
