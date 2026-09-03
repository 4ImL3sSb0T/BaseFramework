/**
 * @file mjc_hal_gfx.cpp
 * @brief MujicaUI HAL backend on Adafruit_GFX + tft_fb (ST7735).
 */

#include "mjc_hal_gfx.h"
#include "mjc_hal.h"
#include "tft_fb.h"

#include <cstdio>
#include <cstring>

static mjc_hal_driver_t s_gfx_drv;
static uint8_t s_text_size = 1;

static uint8_t in_bounds(uint16_t x, uint16_t y)
{
    return (x < s_gfx_drv.width) && (y < s_gfx_drv.height);
}

static uint8_t clip_rect(uint16_t x, uint16_t y, uint16_t *w, uint16_t *h)
{
    if (w == NULL || h == NULL) {
        return 0;
    }
    if (x >= s_gfx_drv.width || y >= s_gfx_drv.height) {
        return 0;
    }
    uint32_t max_w = (uint32_t)s_gfx_drv.width - x;
    uint32_t max_h = (uint32_t)s_gfx_drv.height - y;
    if (*w > max_w) {
        *w = (uint16_t)max_w;
    }
    if (*h > max_h) {
        *h = (uint16_t)max_h;
    }
    return (*w > 0 && *h > 0) ? 1 : 0;
}

static void gfx_clear(void)
{
    tft_fb().fillScreen(s_gfx_drv.bg_color);
}

static void gfx_fill(uint16_t color)
{
    tft_fb().fillScreen(color);
}

static void gfx_set_color(uint16_t pen, uint16_t bg)
{
    s_gfx_drv.pen_color = pen;
    s_gfx_drv.bg_color = bg;
    tft_fb().setTextColor(pen, bg);
}

static void gfx_draw_point(uint16_t x, uint16_t y, uint16_t color)
{
    if (!in_bounds(x, y)) {
        return;
    }
    tft_fb().drawPixel((int16_t)x, (int16_t)y, color);
}

static void gfx_draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    tft_fb().drawLine((int16_t)x0, (int16_t)y0, (int16_t)x1, (int16_t)y1, color);
}

static void gfx_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if (!clip_rect(x, y, &w, &h)) {
        return;
    }
    tft_fb().drawRect((int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h, color);
}

static void gfx_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if (!clip_rect(x, y, &w, &h)) {
        return;
    }
    tft_fb().fillRect((int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h, color);
}

static void gfx_draw_char(uint16_t x, uint16_t y, char c)
{
    if (!in_bounds(x, y)) {
        return;
    }
    Adafruit_GFX &g = tft_fb();
    g.setCursor((int16_t)x, (int16_t)y);
    g.setTextSize(s_text_size);
    g.setTextColor(s_gfx_drv.pen_color, s_gfx_drv.bg_color);
    g.write((uint8_t)c);
}

static void gfx_draw_string(uint16_t x, uint16_t y, const char *str)
{
    if (str == NULL || !in_bounds(x, y)) {
        return;
    }
    Adafruit_GFX &g = tft_fb();
    g.setCursor((int16_t)x, (int16_t)y);
    g.setTextSize(s_text_size);
    g.setTextColor(s_gfx_drv.pen_color, s_gfx_drv.bg_color);
    g.setTextWrap(false);
    g.print(str);
}

static void gfx_set_font_size(uint8_t size)
{
    /* Adafruit built-in font: size 1 = 6x8, 2 = 12x16, 3 = 18x24 */
    if (size < 1u) {
        size = 1u;
    }
    if (size > 3u) {
        size = 3u;
    }
    s_text_size = size;
    tft_fb().setTextSize(s_text_size);
}

static void gfx_present(void)
{
    tft_fb_flush();
}

extern "C" uint8_t mjc_hal_gfx_init(void)
{
    /* rotation 1 → landscape 160x128 on ST7735 */
    if (!tft_fb_init(1, INITR_BLACKTAB)) {
        return 0;
    }

    s_gfx_drv.width = (uint16_t)tft_fb_width();
    s_gfx_drv.height = (uint16_t)tft_fb_height();
    s_gfx_drv.pen_color = 0xFFFF;
    s_gfx_drv.bg_color = 0x0000;
    s_gfx_drv.user_data = NULL;

    s_gfx_drv.clear = gfx_clear;
    s_gfx_drv.fill = gfx_fill;
    s_gfx_drv.set_color = gfx_set_color;
    s_gfx_drv.draw_point = gfx_draw_point;
    s_gfx_drv.draw_line = gfx_draw_line;
    s_gfx_drv.draw_rect = gfx_draw_rect;
    s_gfx_drv.fill_rect = gfx_fill_rect;
    s_gfx_drv.draw_char = gfx_draw_char;
    s_gfx_drv.draw_string = gfx_draw_string;
    s_gfx_drv.set_font_size = gfx_set_font_size;
    s_gfx_drv.present = gfx_present;

    s_text_size = 1;
    tft_fb().setTextSize(1);
    tft_fb().setTextWrap(false);
    tft_fb().setTextColor(s_gfx_drv.pen_color, s_gfx_drv.bg_color);

    return mjc_hal_use_driver(&s_gfx_drv);
}
