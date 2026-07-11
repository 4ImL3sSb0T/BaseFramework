/**
 * @file tft_fb.h
 * @brief ST7735 off-screen frame buffer + manual flush to the panel.
 *
 * Draw with Adafruit_GFX APIs on tft_fb(), then call tft_fb_flush()
 * (or tft_fb_flush_rect) when you want the screen to update.
 */
#ifndef TFT_FB_H
#define TFT_FB_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus

#include "Adafruit_GFX.h"
#include "Adafruit_ST7735.h"

/**
 * @brief Initialise ST7735 and the RGB565 frame buffer.
 * @param rotation  Panel rotation 0..3 (same as Adafruit setRotation).
 * @param tab       INITR_BLACKTAB / GREENTAB / REDTAB / ...
 * @return true on success.
 *
 * The canvas size matches the logical size after @p rotation
 * (e.g. rotation 1 → 160×128). Keep canvas.setRotation(0); use this
 * function's rotation for orientation.
 */
bool tft_fb_init(uint8_t rotation = 1, uint8_t tab = INITR_BLACKTAB);

/** Off-screen drawing target (GFXcanvas16). */
Adafruit_GFX &tft_fb(void);

/** Underlying hardware driver (direct SPI path if needed). */
Adafruit_ST7735 &tft_fb_hw(void);

/** Push the entire frame buffer to the display. */
void tft_fb_flush(void);

/**
 * @brief Push a rectangular region of the buffer to the display.
 * Coordinates are in the current logical space (after init rotation).
 */
void tft_fb_flush_rect(int16_t x, int16_t y, int16_t w, int16_t h);

/** Raw RGB565 pixels (little-endian host order), row-major, stride = width. */
uint16_t *tft_fb_buffer(void);

int16_t tft_fb_width(void);
int16_t tft_fb_height(void);

#endif /* __cplusplus */

#endif /* TFT_FB_H */
