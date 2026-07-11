/**
 * @file tft_demo.cpp
 * @brief ST7735 demo using off-screen buffer + manual flush.
 *
 * Flow:
 *   1) Draw everything into RAM (tft_fb)
 *   2) Push to panel only when ready (tft_fb_flush / flush_rect)
 */
#include "tft_fb.h"
#include "cmsis_os.h"

/* Landscape after rotation 1: 160 x 128 */
static constexpr int16_t BALL_R = 10;

static void draw_static_scene(Adafruit_GFX &g) {
  g.fillScreen(ST77XX_BLACK);

  g.setTextSize(2);
  g.setTextColor(ST77XX_CYAN);
  g.setCursor(8, 4);
  g.print("STM32H750");

  g.setTextSize(1);
  g.setTextColor(ST77XX_WHITE);
  g.setCursor(8, 28);
  g.print("buffer -> flush");

  g.fillRect(10, 48, 28, 28, ST77XX_RED);
  g.fillRect(48, 48, 28, 28, ST77XX_GREEN);
  g.fillRect(86, 48, 28, 28, ST77XX_BLUE);
  g.drawRect(8, 46, 108, 32, ST77XX_YELLOW);

  g.setCursor(8, 88);
  g.setTextColor(ST77XX_MAGENTA);
  g.print("partial dirty rect");
}

/** Erase previous ball and draw new one; return dirty rect. */
static void move_ball(Adafruit_GFX &g, int16_t ox, int16_t oy, int16_t nx,
                      int16_t ny, int16_t &dx, int16_t &dy, int16_t &dw,
                      int16_t &dh) {
  /* Clear old ball area (background). */
  g.fillCircle(ox, oy, BALL_R, ST77XX_BLACK);
  /* Draw new ball. */
  g.fillCircle(nx, ny, BALL_R, ST77XX_ORANGE);
  g.drawCircle(nx, ny, BALL_R, ST77XX_YELLOW);

  /* Union of old/new bounding boxes → flush only this region. */
  const int16_t x0 = (ox < nx) ? (ox - BALL_R) : (nx - BALL_R);
  const int16_t y0 = (oy < ny) ? (oy - BALL_R) : (ny - BALL_R);
  const int16_t x1 = (ox > nx) ? (ox + BALL_R) : (nx + BALL_R);
  const int16_t y1 = (oy > ny) ? (oy + BALL_R) : (ny + BALL_R);
  dx = x0 - 1;
  dy = y0 - 1;
  dw = (x1 - x0) + 3;
  dh = (y1 - y0) + 3;
}

extern "C" void tft_demo_task(void) {
  if (!tft_fb_init(1, INITR_BLACKTAB)) {
    return;
  }

  Adafruit_GFX &g = tft_fb();
  const int16_t W = tft_fb_width();
  const int16_t H = tft_fb_height();

  /* ---------- Phase 1: compose static UI in buffer, one full flush ---------- */
  draw_static_scene(g);
  tft_fb_flush();
  osDelay(500);

  /* ---------- Phase 2: bounce a ball; only dirty rect is pushed each frame ---------- */
  int16_t bx = W / 2;
  int16_t by = H - 20;
  int16_t vx = 3;
  int16_t vy = -2;

  /* Seed ball into buffer and push that patch once. */
  g.fillCircle(bx, by, BALL_R, ST77XX_ORANGE);
  g.drawCircle(bx, by, BALL_R, ST77XX_YELLOW);
  tft_fb_flush_rect(bx - BALL_R - 1, by - BALL_R - 1, BALL_R * 2 + 3,
                    BALL_R * 2 + 3);

  for (int frame = 0; frame < 120; frame++) {
    const int16_t ox = bx;
    const int16_t oy = by;

    bx += vx;
    by += vy;

    /* Bounce inside panel (leave a margin for the ball radius). */
    if (bx < BALL_R + 2) {
      bx = BALL_R + 2;
      vx = -vx;
    } else if (bx > W - BALL_R - 2) {
      bx = W - BALL_R - 2;
      vx = -vx;
    }
    if (by < 86 + BALL_R) {
      /* Keep ball under the static UI block. */
      by = 86 + BALL_R;
      vy = -vy;
    } else if (by > H - BALL_R - 2) {
      by = H - BALL_R - 2;
      vy = -vy;
    }

    int16_t dx, dy, dw, dh;
    move_ball(g, ox, oy, bx, by, dx, dy, dw, dh);
    tft_fb_flush_rect(dx, dy, dw, dh);

    osDelay(30);
  }

  /* ---------- Phase 3: final full-screen message ---------- */
  g.fillScreen(ST77XX_BLACK);
  g.setTextSize(2);
  g.setTextColor(ST77XX_GREEN);
  g.setCursor(16, 40);
  g.println("flush OK");
  g.setTextSize(1);
  g.setTextColor(ST77XX_WHITE);
  g.setCursor(16, 70);
  g.print(W);
  g.print('x');
  g.print(H);
  g.print(" RGB565");
  tft_fb_flush();
}
