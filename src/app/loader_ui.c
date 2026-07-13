/**
 * @file loader_ui.c
 * @brief 电子负载 TFT UI（深色仪表盘）— 见 LOADER_UI.md
 *
 * 当前阶段：不接 loader_runtime，使用本地随机/随机游走模拟数据。
 * 导航：演示自动切页；PC13 用户键短按也可下一页。
 */

#include "loader_task.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "main.h"
#include "mjc_hal.h"
#include "mjc_hal_gfx.h"
#include "service/sys/sys_log.h"

/* -------------------------------------------------------------------------- */
/* 颜色 / 布局（160×128）                                                       */
/* -------------------------------------------------------------------------- */

#define UI_COL_BG        0x0000u
#define UI_COL_CYAN      0x07FFu
#define UI_COL_AMBER     0xFD20u
#define UI_COL_WHITE     0xEF7Du
#define UI_COL_DIM       0x8410u
#define UI_COL_GREEN     0x07E0u
#define UI_COL_RED       0xF800u
#define UI_COL_SELECT    0x0210u
#define UI_COL_GRID      0x4208u
#define UI_COL_PANEL     0x10A2u

#define UI_W             160
#define UI_H             128

#define UI_TOP_H         12
#define UI_READ_Y0       12
#define UI_CHART_Y0      56
#define UI_CHART_Y1      108
#define UI_CHART_X0      4
#define UI_CHART_X1      155
#define UI_BOT_Y0        113

#define UI_CHART_W       ((UI_CHART_X1) - (UI_CHART_X0) + 1)
#define UI_CHART_H       ((UI_CHART_Y1) - (UI_CHART_Y0) + 1)

#define UI_CHART_POINTS  96
#define UI_I_MAX         5.0f
#define UI_P_MAX         50.0f

/** 演示：无四键时自动轮播页面（ms，0=关闭） */
#ifndef UI_DEMO_AUTO_PAGE_MS
#define UI_DEMO_AUTO_PAGE_MS  6000u
#endif

/* -------------------------------------------------------------------------- */
/* 页面 / 模拟状态                                                              */
/* -------------------------------------------------------------------------- */

typedef enum {
    UI_PAGE_HOME = 0,
    UI_PAGE_SET,
    UI_PAGE_STATUS,
    UI_PAGE_CHART,
    UI_PAGE_ABOUT,
    UI_PAGE_COUNT
} ui_page_t;

typedef enum {
    UI_MODE_CC = 0,
    UI_MODE_CV,
    UI_MODE_CP,
    UI_MODE_CR
} ui_mode_t;

typedef enum {
    UI_STATE_IDLE = 0,
    UI_STATE_RUNNING,
    UI_STATE_ERROR
} ui_state_t;

typedef struct {
    ui_mode_t  mode;
    ui_state_t state;
    bool       output_on;
    float      voltage;
    float      current;
    float      power;
    float      temperature;
    float      current_set;
    float      voltage_set;
    float      power_set;
    float      resistance_set;
    float      peak_i;
    float      peak_p;
    const char *fault_text;
} ui_mock_t;

typedef struct {
    float    samples[UI_CHART_POINTS];
    uint16_t head;
    bool     filled;
    bool     paused;
    bool     show_power; /* 全屏图：false=I, true=P */
} ui_chart_t;

static ui_page_t  s_page = UI_PAGE_HOME;
static ui_mock_t  s_mock;
static ui_chart_t s_chart;
static uint32_t   s_tick_ms;
static uint32_t   s_rng = 0xA5A5u;
static uint8_t    s_set_sel;
static uint8_t    s_step_idx;
static bool       s_inited;
static ui_page_t  s_last_drawn = (ui_page_t)0xFF;

static const float s_steps[] = { 0.01f, 0.1f, 1.0f };

/* -------------------------------------------------------------------------- */
/* 工具                                                                        */
/* -------------------------------------------------------------------------- */

static uint32_t ui_rand_u32(void)
{
    s_rng = s_rng * 1664525u + 1013904223u;
    return s_rng;
}

static float ui_randf(float lo, float hi)
{
    float t = (float)(ui_rand_u32() & 0xFFFFu) / 65535.0f;
    return lo + (hi - lo) * t;
}

static float ui_clampf(float v, float lo, float hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static int16_t ui_map_y(float v, float vmin, float vmax, int16_t y_top, int16_t y_bot)
{
    if (vmax <= vmin) {
        return y_bot;
    }
    float t = (v - vmin) / (vmax - vmin);
    if (t < 0.0f) {
        t = 0.0f;
    }
    if (t > 1.0f) {
        t = 1.0f;
    }
    /* 值大 → 更靠上 */
    return (int16_t)(y_bot - (int16_t)(t * (float)(y_bot - y_top)));
}

static const mjc_hal_driver_t *ui_drv(void)
{
    return mjc_hal_get_driver();
}

static void ui_fill(uint16_t color)
{
    const mjc_hal_driver_t *d = ui_drv();
    if (d != NULL && d->fill != NULL) {
        d->fill(color);
    }
}

static void ui_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    const mjc_hal_driver_t *d = ui_drv();
    if (d != NULL && d->fill_rect != NULL) {
        d->fill_rect(x, y, w, h, color);
    }
}

static void ui_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    const mjc_hal_driver_t *d = ui_drv();
    if (d != NULL && d->draw_rect != NULL) {
        d->draw_rect(x, y, w, h, color);
    }
}

static void ui_draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    const mjc_hal_driver_t *d = ui_drv();
    if (d != NULL && d->draw_line != NULL) {
        d->draw_line(x0, y0, x1, y1, color);
    }
}

static void ui_text(uint16_t x, uint16_t y, uint8_t size, uint16_t pen, uint16_t bg, const char *s)
{
    const mjc_hal_driver_t *d = ui_drv();
    if (d == NULL || s == NULL) {
        return;
    }
    if (d->set_font_size != NULL) {
        d->set_font_size(size);
    }
    if (d->set_color != NULL) {
        d->set_color(pen, bg);
    }
    if (d->draw_string != NULL) {
        d->draw_string(x, y, s);
    }
}

static const char *ui_mode_str(ui_mode_t m)
{
    switch (m) {
    case UI_MODE_CC: return "CC";
    case UI_MODE_CV: return "CV";
    case UI_MODE_CP: return "CP";
    case UI_MODE_CR: return "CR";
    default:         return "??";
    }
}

static const char *ui_state_str(ui_state_t s)
{
    switch (s) {
    case UI_STATE_IDLE:    return "IDLE";
    case UI_STATE_RUNNING: return "RUN";
    case UI_STATE_ERROR:   return "FAULT";
    default:               return "??";
    }
}

static float ui_active_setpoint(const ui_mock_t *m)
{
    switch (m->mode) {
    case UI_MODE_CC: return m->current_set;
    case UI_MODE_CV: return m->voltage_set;
    case UI_MODE_CP: return m->power_set;
    case UI_MODE_CR: return m->resistance_set;
    default:         return 0.0f;
    }
}

static const char *ui_set_unit(ui_mode_t m)
{
    switch (m) {
    case UI_MODE_CC: return "A";
    case UI_MODE_CV: return "V";
    case UI_MODE_CP: return "W";
    case UI_MODE_CR: return "R";
    default:         return "";
    }
}

/* -------------------------------------------------------------------------- */
/* 模拟数据                                                                    */
/* -------------------------------------------------------------------------- */

static void ui_mock_init(void)
{
    memset(&s_mock, 0, sizeof(s_mock));
    s_mock.mode        = UI_MODE_CC;
    s_mock.state       = UI_STATE_RUNNING;
    s_mock.output_on   = true;
    s_mock.voltage     = 12.0f;
    s_mock.current     = 1.0f;
    s_mock.power       = 12.0f;
    s_mock.temperature = 28.0f;
    s_mock.current_set = 1.5f;
    s_mock.voltage_set = 5.0f;
    s_mock.power_set   = 10.0f;
    s_mock.resistance_set = 8.0f;
    s_mock.fault_text  = "NONE";
}

static void ui_mock_step(void)
{
    /* 电压：慢随机游走 3~20 V */
    s_mock.voltage += ui_randf(-0.04f, 0.04f);
    s_mock.voltage  = ui_clampf(s_mock.voltage, 3.0f, 20.0f);

    /* 电流：更快波动，便于折线好看；输出关则衰减到 0 */
    if (s_mock.output_on && s_mock.state == UI_STATE_RUNNING) {
        float target = s_mock.current_set;
        /* 向设定靠近 + 噪声 */
        s_mock.current += (target - s_mock.current) * 0.08f;
        s_mock.current += ui_randf(-0.12f, 0.12f);
        /* 偶发尖峰 */
        if ((ui_rand_u32() & 0x3Fu) == 0u) {
            s_mock.current += ui_randf(0.2f, 0.6f);
        }
        s_mock.current = ui_clampf(s_mock.current, 0.0f, UI_I_MAX);
    } else {
        s_mock.current *= 0.85f;
        if (s_mock.current < 0.001f) {
            s_mock.current = 0.0f;
        }
    }

    s_mock.power = s_mock.voltage * s_mock.current;

    s_mock.temperature += ui_randf(-0.05f, 0.08f);
    s_mock.temperature  = ui_clampf(s_mock.temperature, 22.0f, 55.0f);

    if (s_mock.current > s_mock.peak_i) {
        s_mock.peak_i = s_mock.current;
    } else {
        s_mock.peak_i *= 0.999f;
    }
    if (s_mock.power > s_mock.peak_p) {
        s_mock.peak_p = s_mock.power;
    } else {
        s_mock.peak_p *= 0.999f;
    }

    /* 极低概率模拟故障闪一下再恢复（纯演示） */
    if (s_mock.state != UI_STATE_ERROR && (ui_rand_u32() & 0x7FFu) == 0u) {
        s_mock.state = UI_STATE_ERROR;
        s_mock.output_on = false;
        s_mock.fault_text = "OCP";
    } else if (s_mock.state == UI_STATE_ERROR && (ui_rand_u32() & 0x7Fu) == 0u) {
        s_mock.state = UI_STATE_IDLE;
        s_mock.fault_text = "NONE";
    }
}

/* -------------------------------------------------------------------------- */
/* 折线                                                                        */
/* -------------------------------------------------------------------------- */

static void ui_chart_init(void)
{
    memset(&s_chart, 0, sizeof(s_chart));
}

static void ui_chart_push(float v)
{
    if (s_chart.paused) {
        return;
    }
    s_chart.samples[s_chart.head] = v;
    s_chart.head = (uint16_t)((s_chart.head + 1u) % UI_CHART_POINTS);
    if (s_chart.head == 0u) {
        s_chart.filled = true;
    }
}

static void ui_chart_draw(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                          float ymin, float ymax, uint16_t line_color)
{
    const uint16_t w = (uint16_t)(x1 - x0 + 1u);
    const uint16_t h = (uint16_t)(y1 - y0 + 1u);

    ui_fill_rect(x0, y0, w, h, UI_COL_BG);
    ui_draw_rect(x0, y0, w, h, UI_COL_DIM);

    /* 网格：2 横 + 3 竖 */
    for (uint8_t i = 1; i <= 2; i++) {
        uint16_t gy = (uint16_t)(y0 + (h * i) / 3u);
        ui_draw_line(x0 + 1u, gy, x1 - 1u, gy, UI_COL_GRID);
    }
    for (uint8_t i = 1; i <= 3; i++) {
        uint16_t gx = (uint16_t)(x0 + (w * i) / 4u);
        ui_draw_line(gx, y0 + 1u, gx, y1 - 1u, UI_COL_GRID);
    }

    uint16_t n = s_chart.filled ? UI_CHART_POINTS : s_chart.head;
    if (n < 2u) {
        return;
    }

    uint16_t start = s_chart.filled
                         ? s_chart.head
                         : 0u;

    int16_t prev_x = (int16_t)(x0 + 1);
    int16_t prev_y = ui_map_y(s_chart.samples[start % UI_CHART_POINTS],
                              ymin, ymax, (int16_t)(y0 + 1), (int16_t)(y1 - 1));

    for (uint16_t i = 1; i < n; i++) {
        uint16_t idx = (uint16_t)((start + i) % UI_CHART_POINTS);
        int16_t x = (int16_t)(x0 + 1 + (int32_t)i * (int32_t)(w - 3) / (int32_t)(n - 1));
        int16_t y = ui_map_y(s_chart.samples[idx], ymin, ymax,
                             (int16_t)(y0 + 1), (int16_t)(y1 - 1));
        ui_draw_line((uint16_t)prev_x, (uint16_t)prev_y, (uint16_t)x, (uint16_t)y, line_color);
        prev_x = x;
        prev_y = y;
    }
}

/* -------------------------------------------------------------------------- */
/* 公共装饰                                                                    */
/* -------------------------------------------------------------------------- */

static void ui_draw_tab_bar(ui_page_t active)
{
    static const char *const tabs[UI_PAGE_COUNT] = { "Home", "Set", "Sts", "Plot", "Info" };
    ui_fill_rect(0, UI_BOT_Y0, UI_W, (uint16_t)(UI_H - UI_BOT_Y0), UI_COL_BG);

    uint16_t x = 2;
    for (uint8_t i = 0; i < UI_PAGE_COUNT; i++) {
        uint16_t pen = (i == (uint8_t)active) ? UI_COL_CYAN : UI_COL_DIM;
        char mark[12];
        (void)snprintf(mark, sizeof(mark), "%c%s", (i == (uint8_t)active) ? '*' : ' ', tabs[i]);
        ui_text(x, (uint16_t)(UI_BOT_Y0 + 3u), 1, pen, UI_COL_BG, mark);
        x = (uint16_t)(x + 32u);
    }
}

static void ui_draw_top_bar_home(void)
{
    char buf[40];

    if (s_mock.state == UI_STATE_ERROR) {
        ui_fill_rect(0, 0, UI_W, UI_TOP_H, UI_COL_RED);
        (void)snprintf(buf, sizeof(buf), "FAULT:%s", s_mock.fault_text);
        ui_text(4, 2, 1, UI_COL_WHITE, UI_COL_RED, buf);
        return;
    }

    ui_fill_rect(0, 0, UI_W, UI_TOP_H, UI_COL_BG);
    (void)snprintf(buf, sizeof(buf), "%s %s", ui_mode_str(s_mock.mode), ui_state_str(s_mock.state));
    ui_text(2, 2, 1, UI_COL_CYAN, UI_COL_BG, buf);

    (void)snprintf(buf, sizeof(buf), "%4.1fC", (double)s_mock.temperature);
    ui_text(78, 2, 1, UI_COL_DIM, UI_COL_BG, buf);

    if (s_mock.output_on) {
        ui_fill_rect(128, 1, 28, 10, UI_COL_GREEN);
        ui_text(132, 2, 1, UI_COL_BG, UI_COL_GREEN, "ON");
    } else {
        ui_fill_rect(128, 1, 28, 10, UI_COL_DIM);
        ui_text(130, 2, 1, UI_COL_WHITE, UI_COL_DIM, "OFF");
    }
}

static void ui_draw_page_header(const char *title, ui_page_t page)
{
    char buf[24];
    ui_fill_rect(0, 0, UI_W, UI_TOP_H, UI_COL_BG);
    (void)snprintf(buf, sizeof(buf), "<%s  %u/%u", title, (unsigned)page + 1u, (unsigned)UI_PAGE_COUNT);
    ui_text(2, 2, 1, UI_COL_CYAN, UI_COL_BG, buf);
    ui_draw_line(0, (uint16_t)(UI_TOP_H - 1u), UI_W - 1u, (uint16_t)(UI_TOP_H - 1u), UI_COL_GRID);
}

/* -------------------------------------------------------------------------- */
/* 各页绘制                                                                    */
/* -------------------------------------------------------------------------- */

static void ui_draw_home(void)
{
    char buf[32];

    ui_fill(UI_COL_BG);
    ui_draw_top_bar_home();

    /* 电压大字 */
    (void)snprintf(buf, sizeof(buf), "%6.3f", (double)s_mock.voltage);
    ui_text(4, 14, 3, UI_COL_CYAN, UI_COL_BG, buf);
    ui_text(118, 22, 2, UI_COL_DIM, UI_COL_BG, "V");

    /* 电流 / 功率 */
    (void)snprintf(buf, sizeof(buf), "%5.3fA", (double)s_mock.current);
    ui_text(4, 40, 2, UI_COL_AMBER, UI_COL_BG, buf);

    (void)snprintf(buf, sizeof(buf), "%5.2fW", (double)s_mock.power);
    ui_text(90, 40, 2, UI_COL_WHITE, UI_COL_BG, buf);

    /* 设定一行 */
    (void)snprintf(buf, sizeof(buf), "Set %5.3f%s",
                   (double)ui_active_setpoint(&s_mock), ui_set_unit(s_mock.mode));
    ui_text(4, 48, 1, UI_COL_DIM, UI_COL_BG, buf);

    /* 折线 */
    ui_chart_draw(UI_CHART_X0, UI_CHART_Y0, UI_CHART_X1, UI_CHART_Y1,
                  0.0f, UI_I_MAX, UI_COL_AMBER);

    ui_text(4, (uint16_t)(UI_CHART_Y1 + 1u), 1, UI_COL_DIM, UI_COL_BG, "I 0");
    ui_text(70, (uint16_t)(UI_CHART_Y1 + 1u), 1, UI_COL_DIM, UI_COL_BG, "2.5");
    ui_text(130, (uint16_t)(UI_CHART_Y1 + 1u), 1, UI_COL_DIM, UI_COL_BG, "5A");

    ui_draw_tab_bar(UI_PAGE_HOME);
}

static void ui_draw_set(void)
{
    char buf[40];
    static const char *const items[] = {
        "Mode", "Setpoint", "Output", "Step", "Back home"
    };
    const uint8_t n = 5;

    ui_fill(UI_COL_BG);
    ui_draw_page_header("SET", UI_PAGE_SET);

    for (uint8_t i = 0; i < n; i++) {
        uint16_t y = (uint16_t)(16u + i * 14u);
        bool sel = (i == s_set_sel);
        uint16_t bg = sel ? UI_COL_SELECT : UI_COL_BG;
        uint16_t pen = sel ? UI_COL_WHITE : UI_COL_DIM;

        if (sel) {
            ui_fill_rect(0, y, UI_W, 13, bg);
        }

        switch (i) {
        case 0:
            (void)snprintf(buf, sizeof(buf), "%c %-8s [%s]", sel ? '>' : ' ',
                           items[i], ui_mode_str(s_mock.mode));
            break;
        case 1:
            (void)snprintf(buf, sizeof(buf), "%c %-8s %6.3f%s", sel ? '>' : ' ',
                           items[i], (double)ui_active_setpoint(&s_mock),
                           ui_set_unit(s_mock.mode));
            break;
        case 2:
            (void)snprintf(buf, sizeof(buf), "%c %-8s %s", sel ? '>' : ' ',
                           items[i], s_mock.output_on ? "ON" : "OFF");
            break;
        case 3:
            (void)snprintf(buf, sizeof(buf), "%c %-8s %.2f", sel ? '>' : ' ',
                           items[i], (double)s_steps[s_step_idx % 3u]);
            break;
        default:
            (void)snprintf(buf, sizeof(buf), "%c %s", sel ? '>' : ' ', items[i]);
            break;
        }
        ui_text(2, (uint16_t)(y + 2u), 1, pen, bg, buf);
    }

    ui_text(2, 100, 1, UI_COL_DIM, UI_COL_BG, "SIM data  KEY:next page");
    ui_draw_tab_bar(UI_PAGE_SET);
}

static void ui_draw_status(void)
{
    char buf[40];

    ui_fill(UI_COL_BG);
    ui_draw_page_header("STATUS", UI_PAGE_STATUS);

    (void)snprintf(buf, sizeof(buf), "State  %s", ui_state_str(s_mock.state));
    ui_text(4, 16, 1, UI_COL_WHITE, UI_COL_BG, buf);

    (void)snprintf(buf, sizeof(buf), "Fault  %s", s_mock.fault_text);
    ui_text(4, 28, 1,
            (s_mock.state == UI_STATE_ERROR) ? UI_COL_RED : UI_COL_DIM,
            UI_COL_BG, buf);

    (void)snprintf(buf, sizeof(buf), "V  %7.3f   I %6.3f",
                   (double)s_mock.voltage, (double)s_mock.current);
    ui_text(4, 42, 1, UI_COL_CYAN, UI_COL_BG, buf);

    (void)snprintf(buf, sizeof(buf), "P  %7.2f   T %5.1fC",
                   (double)s_mock.power, (double)s_mock.temperature);
    ui_text(4, 54, 1, UI_COL_AMBER, UI_COL_BG, buf);

    (void)snprintf(buf, sizeof(buf), "OCP  %.1fA   OTP 50C", (double)UI_I_MAX);
    ui_text(4, 70, 1, UI_COL_DIM, UI_COL_BG, buf);

    (void)snprintf(buf, sizeof(buf), "PeakI %.2f  PeakP %.1f",
                   (double)s_mock.peak_i, (double)s_mock.peak_p);
    ui_text(4, 82, 1, UI_COL_DIM, UI_COL_BG, buf);

    ui_text(4, 98, 1, UI_COL_DIM, UI_COL_BG, "Mock protect display");
    ui_draw_tab_bar(UI_PAGE_STATUS);
}

static void ui_draw_chart_full(void)
{
    char buf[48];
    float ymin = 0.0f;
    float ymax;
    uint16_t color;
    const char *ch;

    ui_fill(UI_COL_BG);

    if (s_chart.show_power) {
        ymax = UI_P_MAX;
        color = UI_COL_WHITE;
        ch = "P";
        (void)snprintf(buf, sizeof(buf), "P 5s %s  %.2fW pk%.1f",
                       s_chart.paused ? "HOLD" : "RUN",
                       (double)s_mock.power, (double)s_mock.peak_p);
    } else {
        ymax = UI_I_MAX;
        color = UI_COL_AMBER;
        ch = "I";
        (void)snprintf(buf, sizeof(buf), "I 5s %s  %.3fA pk%.2f",
                       s_chart.paused ? "HOLD" : "RUN",
                       (double)s_mock.current, (double)s_mock.peak_i);
    }
    ui_text(2, 2, 1, UI_COL_CYAN, UI_COL_BG, buf);

    ui_chart_draw(2, 14, 157, 108, ymin, ymax, color);

    (void)snprintf(buf, sizeof(buf), "Y:0..%.0f%s  auto-page / KEY",
                   (double)ymax, ch);
    ui_text(2, 112, 1, UI_COL_DIM, UI_COL_BG, buf);
    (void)ch;
}

static void ui_draw_about(void)
{
    ui_fill(UI_COL_BG);
    ui_draw_page_header("ABOUT", UI_PAGE_ABOUT);

    ui_text(4, 18, 1, UI_COL_WHITE, UI_COL_BG, "Electronic Load");
    ui_text(4, 30, 1, UI_COL_CYAN, UI_COL_BG, "BaseFramework H750");
    ui_text(4, 44, 1, UI_COL_DIM, UI_COL_BG, "Range  5.0A / ~50W");
    ui_text(4, 56, 1, UI_COL_DIM, UI_COL_BG, "UI     v0.1 dark-dash");
    ui_text(4, 68, 1, UI_COL_DIM, UI_COL_BG, "Data   SIMULATED");
    ui_text(4, 80, 1, UI_COL_DIM, UI_COL_BG, "TFT    ST7735 160x128");
    ui_text(4, 98, 1, UI_COL_AMBER, UI_COL_BG, "Runtime not linked");
    ui_draw_tab_bar(UI_PAGE_ABOUT);
}

static void ui_draw_page(void)
{
    switch (s_page) {
    case UI_PAGE_HOME:   ui_draw_home();       break;
    case UI_PAGE_SET:    ui_draw_set();        break;
    case UI_PAGE_STATUS: ui_draw_status();     break;
    case UI_PAGE_CHART:  ui_draw_chart_full(); break;
    case UI_PAGE_ABOUT:  ui_draw_about();      break;
    default:             ui_draw_home();       break;
    }
    s_last_drawn = s_page;
}

/* -------------------------------------------------------------------------- */
/* 输入：PC13 下一页；演示自动轮播                                               */
/* -------------------------------------------------------------------------- */

static void ui_page_next(void)
{
    s_page = (ui_page_t)(((unsigned)s_page + 1u) % (unsigned)UI_PAGE_COUNT);
    /* 设定页演示：顺带移动选中项 */
    if (s_page == UI_PAGE_SET) {
        s_set_sel = (uint8_t)((s_set_sel + 1u) % 5u);
    }
    if (s_page == UI_PAGE_CHART) {
        s_chart.show_power = !s_chart.show_power;
    }
}

static void ui_poll_user_key(void)
{
    /* 板载用户键 PC13：按下=低（多数板）边沿，带简单消抖 */
    static uint8_t stable = 1u;
    static uint8_t last_raw = 1u;
    static uint8_t debounce = 0u;

    uint8_t raw = (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_SET) ? 1u : 0u;

    if (raw != last_raw) {
        last_raw = raw;
        debounce = 3u; /* ~3 poll */
        return;
    }
    if (debounce > 0u) {
        debounce--;
        return;
    }
    if (raw != stable) {
        /* 下降沿：按下 */
        if (stable == 1u && raw == 0u) {
            ui_page_next();
        }
        stable = raw;
    }
}

static void ui_poll_demo_auto_page(void)
{
#if UI_DEMO_AUTO_PAGE_MS > 0
    static uint32_t last_switch;

    if ((s_tick_ms - last_switch) >= UI_DEMO_AUTO_PAGE_MS) {
        last_switch = s_tick_ms;
        ui_page_next();
    }
#else
    (void)s_tick_ms;
#endif
}

/* -------------------------------------------------------------------------- */
/* 对外 API                                                                    */
/* -------------------------------------------------------------------------- */

void loader_ui_init(void)
{
    if (s_inited) {
        return;
    }

    if (!mjc_hal_gfx_init()) {
        sys_log_text(error, "loader_ui: mjc_hal_gfx_init fail");
        return;
    }

    ui_mock_init();
    ui_chart_init();
    s_page = UI_PAGE_HOME;
    s_set_sel = 0;
    s_step_idx = 1; /* 0.1 */
    s_tick_ms = 0;
    s_last_drawn = (ui_page_t)0xFF;

    ui_fill(UI_COL_BG);
    ui_draw_page();
    mjc_hal_present();

    s_inited = true;
    sys_log_text(info, "loader_ui init OK (SIM mode) %ux%u",
                 (unsigned)mjc_hal_screen_width(),
                 (unsigned)mjc_hal_screen_height());
}

void loader_ui_poll(void)
{
    if (!s_inited) {
        loader_ui_init();
        if (!s_inited) {
            return;
        }
    }

    s_tick_ms += 40u; /* 与 loader_task 默认 poll 周期一致 */

    ui_mock_step();

    /* 主页/全屏图推点：主页永远记电流；全屏按通道 */
    if (!s_chart.paused) {
        if (s_page == UI_PAGE_CHART && s_chart.show_power) {
            ui_chart_push(s_mock.power);
        } else {
            ui_chart_push(s_mock.current);
        }
    }

    ui_poll_user_key();
    ui_poll_demo_auto_page();

    /* 每帧整屏重绘 */
    ui_draw_page();
    mjc_hal_present();
}
