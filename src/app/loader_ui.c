/**
 * @file loader_ui.c
 * @brief 电子负载 TFT UI（深色仪表盘）— 见 LOADER_UI.md
 *
 * 数据：loader_runtime（测量只读，设定走 runtime 写口 + core request）
 * 输入：multi_button + bsp_gpio（PC8/PD14/PC7/PC6 = UP/DOWN/ENT/BACK）
 */

#include "loader_task.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "loader_runtime.h"
#include "loader_core.h"
#include "loader_config.h"
#include "bsp/gpio/bsp_gpio.h"
#include "multi_button.h"
#include "mjc_hal.h"
#include "mjc_hal_gfx.h"
#include "bsp/sys/sys_log.h"
#include "FreeRTOS.h"
#include "task.h"

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

#define UI_W             160
#define UI_H             128

#define UI_TOP_H         12
#define UI_CHART_Y0      56
#define UI_CHART_Y1      108
#define UI_CHART_X0      4
#define UI_CHART_X1      155
#define UI_BOT_Y0        113

#define UI_CHART_POINTS  96
#define UI_I_MAX         LOADER_CURRENT_MAX
#define UI_P_MAX         LOADER_POWER_MAX

#define UI_KEY_Q_SIZE    16u
#define UI_BTN_COUNT     4u

/* -------------------------------------------------------------------------- */
/* 页面 / 导航                                                                  */
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
    UI_KEY_UP = 0,
    UI_KEY_DOWN,
    UI_KEY_ENT,
    UI_KEY_BACK,
    UI_KEY_UP_LONG,   /* 长按开始：Set/Chart 切页 */
    UI_KEY_DOWN_LONG
} ui_key_t;

typedef struct {
    float    samples[UI_CHART_POINTS];
    uint16_t head;
    bool     filled;
    bool     paused;
    bool     show_power; /* 全屏图：false=I, true=P */
} ui_chart_t;

static ui_page_t  s_page = UI_PAGE_HOME;
static ui_chart_t s_chart;
static uint8_t    s_set_sel;
static uint8_t    s_step_idx;
static bool       s_editing;
static float      s_edit_value;
static float      s_edit_backup;
static float      s_peak_i;
static float      s_peak_p;
static bool       s_inited;
static ui_page_t  s_last_drawn = (ui_page_t)0xFF;

static const float s_steps[] = { 0.01f, 0.1f, 1.0f };

/* 按键事件队列：timer 任务写，UI 任务读 */
static volatile uint8_t s_key_q[UI_KEY_Q_SIZE];
static volatile uint8_t s_key_wr;
static volatile uint8_t s_key_rd;

static Button s_btns[UI_BTN_COUNT];

/* -------------------------------------------------------------------------- */
/* 工具                                                                        */
/* -------------------------------------------------------------------------- */

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

static const char *ui_mode_str(loader_mode_t m)
{
    switch (m) {
    case LOADER_MODE_CC: return "CC";
    case LOADER_MODE_CV: return "CV";
    case LOADER_MODE_CP: return "CP";
    case LOADER_MODE_CR: return "CR";
    default:             return "??";
    }
}

static const char *ui_state_str(loader_state_t s)
{
    switch (s) {
    case LOADER_STATE_IDLE:    return "IDLE";
    case LOADER_STATE_RUNNING: return "RUN";
    case LOADER_STATE_PAUSED:  return "PAUSE";
    case LOADER_STATE_ERROR:   return "FAULT";
    default:                   return "??";
    }
}

static const char *ui_fault_str(loader_error_t e)
{
    switch (e) {
    case LOADER_ERROR_NONE:            return "NONE";
    case LOADER_ERROR_OVERCURRENT:     return "OCP";
    case LOADER_ERROR_OVERTEMPERATURE: return "OTP";
    case LOADER_ERROR_UNDERVOLTAGE:    return "UVP";
    default:                           return "ERR";
    }
}

static float ui_active_setpoint(const loader_runtime_t *rt)
{
    switch (rt->mode) {
    case LOADER_MODE_CC: return rt->current_setpoint;
    case LOADER_MODE_CV: return rt->voltage_setpoint;
    case LOADER_MODE_CP: return rt->power_setpoint;
    case LOADER_MODE_CR: return rt->resistance_setpoint;
    default:             return 0.0f;
    }
}

static const char *ui_set_unit(loader_mode_t m)
{
    switch (m) {
    case LOADER_MODE_CC: return "A";
    case LOADER_MODE_CV: return "V";
    case LOADER_MODE_CP: return "W";
    case LOADER_MODE_CR: return "R";
    default:             return "";
    }
}

static float ui_setpoint_max(loader_mode_t m)
{
    switch (m) {
    case LOADER_MODE_CC: return LOADER_CURRENT_MAX;
    case LOADER_MODE_CV: return LOADER_VOLTAGE_MAX;
    case LOADER_MODE_CP: return LOADER_POWER_MAX;
    case LOADER_MODE_CR: return LOADER_RESISTANCE_MAX;
    default:             return LOADER_CURRENT_MAX;
    }
}

static float ui_setpoint_min(loader_mode_t m)
{
    if (m == LOADER_MODE_CR) {
        return LOADER_RESISTANCE_EPSILON;
    }
    return 0.0f;
}

static void ui_write_active_setpoint(loader_mode_t mode, float value)
{
    value = ui_clampf(value, ui_setpoint_min(mode), ui_setpoint_max(mode));
    switch (mode) {
    case LOADER_MODE_CC:
        loader_runtime_set_current_setpoint(value);
        break;
    case LOADER_MODE_CV:
        loader_runtime_set_voltage_setpoint(value);
        break;
    case LOADER_MODE_CP:
        loader_runtime_set_power_setpoint(value);
        break;
    case LOADER_MODE_CR:
        loader_runtime_set_resistance_setpoint(value);
        break;
    default:
        break;
    }
}

static void ui_cycle_mode(void)
{
    loader_runtime_t rt = loader_runtime_get();
    loader_mode_t next = (loader_mode_t)(((unsigned)rt.mode + 1u) % 4u);
    loader_runtime_set_mode(next);
}

static void ui_toggle_output(void)
{
    loader_runtime_t rt = loader_runtime_get();
    if (rt.state == LOADER_STATE_RUNNING) {
        (void)loader_core_request_stop();
    } else if (rt.state == LOADER_STATE_ERROR) {
        /* 故障态 ON 会失败；保持 FAULT 显示 */
        (void)loader_core_request_run();
    } else {
        (void)loader_core_request_run();
    }
}

/* -------------------------------------------------------------------------- */
/* 按键队列 / multi_button                                                      */
/* -------------------------------------------------------------------------- */

static void ui_key_push(ui_key_t key)
{
    taskENTER_CRITICAL();
    uint8_t next = (uint8_t)((s_key_wr + 1u) % UI_KEY_Q_SIZE);
    if (next != s_key_rd) {
        s_key_q[s_key_wr] = (uint8_t)key;
        s_key_wr = next;
    }
    taskEXIT_CRITICAL();
}

static bool ui_key_pop(ui_key_t *key)
{
    bool ok = false;
    taskENTER_CRITICAL();
    if (s_key_rd != s_key_wr) {
        *key = (ui_key_t)s_key_q[s_key_rd];
        s_key_rd = (uint8_t)((s_key_rd + 1u) % UI_KEY_Q_SIZE);
        ok = true;
    }
    taskEXIT_CRITICAL();
    return ok;
}

static uint8_t ui_read_button_level(uint8_t id)
{
    return bsp_gpio_key_level(id);
}

/* 长按已消费：松手 PRESS_UP 时不再当短按（bit = button_id） */
static uint8_t s_key_long_consumed;

static void ui_btn_on_click(Button *btn)
{
    if (btn == NULL || btn->button_id >= UI_BTN_COUNT) {
        return;
    }
    /* ENT / BACK：单击确认，避免误触（等 SHORT 窗口，防双击毛刺） */
    if (btn->button_id == (uint8_t)UI_KEY_ENT ||
        btn->button_id == (uint8_t)UI_KEY_BACK) {
        ui_key_push((ui_key_t)btn->button_id);
    }
}

static void ui_btn_on_long_start(Button *btn)
{
    if (btn == NULL || btn->button_id >= UI_BTN_COUNT) {
        return;
    }
    s_key_long_consumed |= (uint8_t)(1u << btn->button_id);

    /* 长按开始：推 LONG（Set/Chart 切页；编辑态当步进） */
    if (btn->button_id == (uint8_t)UI_KEY_UP) {
        ui_key_push(UI_KEY_UP_LONG);
    } else if (btn->button_id == (uint8_t)UI_KEY_DOWN) {
        ui_key_push(UI_KEY_DOWN_LONG);
    }
}

static void ui_btn_on_press_up(Button *btn)
{
    if (btn == NULL || btn->button_id >= UI_BTN_COUNT) {
        return;
    }

    {
        uint8_t bit = (uint8_t)(1u << btn->button_id);
        if ((s_key_long_consumed & bit) != 0u) {
            s_key_long_consumed = (uint8_t)(s_key_long_consumed & (uint8_t)~bit);
            return; /* 已由长按处理 */
        }
    }

    /* 短按松手即响应（不走 SINGLE_CLICK 的 300ms 等待） */
    if (btn->button_id == (uint8_t)UI_KEY_UP ||
        btn->button_id == (uint8_t)UI_KEY_DOWN) {
        ui_key_push((ui_key_t)btn->button_id);
    }
}

static void ui_btn_on_hold(Button *btn)
{
    static uint8_t hold_div[UI_BTN_COUNT];

    if (btn == NULL || btn->button_id >= UI_BTN_COUNT) {
        return;
    }
    if (btn->button_id != (uint8_t)UI_KEY_UP &&
        btn->button_id != (uint8_t)UI_KEY_DOWN) {
        return;
    }
    /* 仅编辑数值时连发；Set/Chart 长按只切一次页，不连翻 */
    if (!s_editing) {
        return;
    }
    /* 长按保持：每 ~50ms 再推一次（tick=5ms） */
    if (++hold_div[btn->button_id] < 10u) {
        return;
    }
    hold_div[btn->button_id] = 0u;
    ui_key_push((ui_key_t)btn->button_id);
}

/**
 * 注册四键到 multi_button。
 * 扫描由中频 loader_state 任务调用 button_ticks()（5ms），此处不建定时器。
 *
 * UP/DOWN：松手短按 = 本页操作；长按开始 = 切页（Set/Chart）；编辑态长按连发步进。
 * ENT/BACK：仅单击。
 */
static void ui_buttons_init(void)
{
    (void)bsp_gpio_init();

    s_key_wr = 0;
    s_key_rd = 0;
    s_key_long_consumed = 0;

    for (uint8_t i = 0; i < UI_BTN_COUNT; i++) {
        button_init(&s_btns[i], ui_read_button_level, BSP_KEY_ACTIVE_LEVEL, i);
        button_attach(&s_btns[i], BTN_PRESS_UP, ui_btn_on_press_up);
        button_attach(&s_btns[i], BTN_SINGLE_CLICK, ui_btn_on_click);
        button_attach(&s_btns[i], BTN_LONG_PRESS_START, ui_btn_on_long_start);
        button_attach(&s_btns[i], BTN_LONG_PRESS_HOLD, ui_btn_on_hold);
        (void)button_start(&s_btns[i]);
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

    uint16_t start = s_chart.filled ? s_chart.head : 0u;
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

/** 选中/编辑态配色：靠背景色突出，不用 * / > 等前缀符号 */
static void ui_sel_style(bool selected, bool editing, uint16_t *pen, uint16_t *bg)
{
    if (editing) {
        *bg  = UI_COL_SELECT;
        *pen = UI_COL_AMBER;
    } else if (selected) {
        *bg  = UI_COL_SELECT;
        *pen = UI_COL_WHITE;
    } else {
        *bg  = UI_COL_BG;
        *pen = UI_COL_DIM;
    }
}

static void ui_draw_tab_bar(ui_page_t active)
{
    static const char *const tabs[UI_PAGE_COUNT] = { "Home", "Set", "Sts", "Plot", "Info" };
    const uint16_t bar_h = (uint16_t)(UI_H - UI_BOT_Y0);
    const uint16_t tab_w = (uint16_t)(UI_W / UI_PAGE_COUNT);

    ui_fill_rect(0, UI_BOT_Y0, UI_W, bar_h, UI_COL_BG);

    for (uint8_t i = 0; i < UI_PAGE_COUNT; i++) {
        bool sel = (i == (uint8_t)active);
        uint16_t pen;
        uint16_t bg;
        uint16_t x = (uint16_t)(i * tab_w);

        ui_sel_style(sel, false, &pen, &bg);
        if (sel) {
            pen = UI_COL_CYAN; /* 当前页签用青色字 + 选中底 */
            ui_fill_rect(x, UI_BOT_Y0, tab_w, bar_h, bg);
        }
        /* size1 约 6px/字；4 字宽 ~24，略居中 */
        ui_text((uint16_t)(x + 4u), (uint16_t)(UI_BOT_Y0 + 3u), 1, pen, bg, tabs[i]);
    }
}

static void ui_draw_top_bar_home(const loader_runtime_t *rt)
{
    char buf[40];
    bool on = (rt->state == LOADER_STATE_RUNNING);

    if (rt->state == LOADER_STATE_ERROR) {
        ui_fill_rect(0, 0, UI_W, UI_TOP_H, UI_COL_RED);
        (void)snprintf(buf, sizeof(buf), "FAULT:%s", ui_fault_str(rt->error));
        ui_text(4, 2, 1, UI_COL_WHITE, UI_COL_RED, buf);
        return;
    }

    ui_fill_rect(0, 0, UI_W, UI_TOP_H, UI_COL_BG);
    (void)snprintf(buf, sizeof(buf), "%s %s", ui_mode_str(rt->mode), ui_state_str(rt->state));
    ui_text(2, 2, 1, UI_COL_CYAN, UI_COL_BG, buf);

    (void)snprintf(buf, sizeof(buf), "%4.1fC", (double)rt->temperature_measurement);
    ui_text(78, 2, 1, UI_COL_DIM, UI_COL_BG, buf);

    if (on) {
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

static void ui_draw_home(const loader_runtime_t *rt)
{
    char buf[32];
    float set_v = s_editing ? s_edit_value : ui_active_setpoint(rt);

    ui_fill(UI_COL_BG);
    ui_draw_top_bar_home(rt);

    (void)snprintf(buf, sizeof(buf), "%6.3f", (double)rt->voltage_measurement);
    ui_text(4, 14, 3, UI_COL_CYAN, UI_COL_BG, buf);
    ui_text(118, 22, 2, UI_COL_DIM, UI_COL_BG, "V");

    (void)snprintf(buf, sizeof(buf), "%5.3fA", (double)rt->current_measurement);
    ui_text(4, 40, 2, UI_COL_AMBER, UI_COL_BG, buf);

    (void)snprintf(buf, sizeof(buf), "%5.2fW", (double)rt->power_measurement);
    ui_text(90, 40, 2, UI_COL_WHITE, UI_COL_BG, buf);

    ui_chart_draw(UI_CHART_X0, UI_CHART_Y0, UI_CHART_X1, UI_CHART_Y1,
                  0.0f, UI_I_MAX, UI_COL_AMBER);

    (void)snprintf(buf, sizeof(buf), "Set %5.3f%s",
                   (double)set_v, ui_set_unit(rt->mode));
    ui_text((uint16_t)(UI_CHART_X0 + 2u), (uint16_t)(UI_CHART_Y0 + 2u),
            1, UI_COL_DIM, UI_COL_BG, buf);

    ui_text(4, (uint16_t)(UI_CHART_Y1 + 1u), 1, UI_COL_DIM, UI_COL_BG, "I 0");
    ui_text(70, (uint16_t)(UI_CHART_Y1 + 1u), 1, UI_COL_DIM, UI_COL_BG, "2.5");
    ui_text(130, (uint16_t)(UI_CHART_Y1 + 1u), 1, UI_COL_DIM, UI_COL_BG, "5A");

    ui_draw_tab_bar(UI_PAGE_HOME);
}

static void ui_draw_set(const loader_runtime_t *rt)
{
    char buf[40];
    static const char *const items[] = {
        "Mode", "Setpoint", "Output", "Step", "Back home"
    };
    const uint8_t n = 5;
    bool on = (rt->state == LOADER_STATE_RUNNING);
    float set_v = s_editing ? s_edit_value : ui_active_setpoint(rt);

    ui_fill(UI_COL_BG);
    ui_draw_page_header("SET", UI_PAGE_SET);

    for (uint8_t i = 0; i < n; i++) {
        uint16_t y = (uint16_t)(16u + i * 14u);
        bool sel = (i == s_set_sel);
        bool editing = (sel && s_editing && i == 1u);
        uint16_t pen;
        uint16_t bg;

        ui_sel_style(sel, editing, &pen, &bg);
        if (sel) {
            ui_fill_rect(0, y, UI_W, 13, bg);
        }

        switch (i) {
        case 0:
            (void)snprintf(buf, sizeof(buf), "%-8s [%s]",
                           items[i], ui_mode_str(rt->mode));
            break;
        case 1:
            (void)snprintf(buf, sizeof(buf), "%-8s %6.3f%s",
                           items[i], (double)set_v, ui_set_unit(rt->mode));
            break;
        case 2:
            (void)snprintf(buf, sizeof(buf), "%-8s %s",
                           items[i], on ? "ON" : "OFF");
            break;
        case 3:
            (void)snprintf(buf, sizeof(buf), "%-8s %.2f",
                           items[i], (double)s_steps[s_step_idx % 3u]);
            break;
        default:
            (void)snprintf(buf, sizeof(buf), "%s", items[i]);
            break;
        }
        ui_text(4, (uint16_t)(y + 2u), 1, pen, bg, buf);
    }

    ui_text(2, 100, 1, UI_COL_DIM, UI_COL_BG,
            s_editing ? "Edit: UP/DN step  ENT:save"
                      : "ENT:act holdUP/DN:page");
    ui_draw_tab_bar(UI_PAGE_SET);
}

static void ui_draw_status(const loader_runtime_t *rt)
{
    char buf[40];

    ui_fill(UI_COL_BG);
    ui_draw_page_header("STATUS", UI_PAGE_STATUS);

    (void)snprintf(buf, sizeof(buf), "State  %s", ui_state_str(rt->state));
    ui_text(4, 16, 1, UI_COL_WHITE, UI_COL_BG, buf);

    (void)snprintf(buf, sizeof(buf), "Fault  %s", ui_fault_str(rt->error));
    ui_text(4, 28, 1,
            (rt->state == LOADER_STATE_ERROR) ? UI_COL_RED : UI_COL_DIM,
            UI_COL_BG, buf);

    (void)snprintf(buf, sizeof(buf), "V  %7.3f   I %6.3f",
                   (double)rt->voltage_measurement, (double)rt->current_measurement);
    ui_text(4, 42, 1, UI_COL_CYAN, UI_COL_BG, buf);

    (void)snprintf(buf, sizeof(buf), "P  %7.2f   T %5.1fC",
                   (double)rt->power_measurement, (double)rt->temperature_measurement);
    ui_text(4, 54, 1, UI_COL_AMBER, UI_COL_BG, buf);

    (void)snprintf(buf, sizeof(buf), "OCP %.1fA  OTP %.0fC",
                   (double)LOADER_OVERCURRENT_LIMIT,
                   (double)LOADER_OVERTEMPERATURE_LIMIT);
    ui_text(4, 70, 1, UI_COL_DIM, UI_COL_BG, buf);

    (void)snprintf(buf, sizeof(buf), "PeakI %.2f  PeakP %.1f",
                   (double)s_peak_i, (double)s_peak_p);
    ui_text(4, 82, 1, UI_COL_DIM, UI_COL_BG, buf);

    if (rt->state == LOADER_STATE_ERROR) {
        ui_text(4, 98, 1, UI_COL_RED, UI_COL_BG, "ENT: clear fault");
    } else {
        ui_text(4, 98, 1, UI_COL_DIM, UI_COL_BG, "BACK: home");
    }
    ui_draw_tab_bar(UI_PAGE_STATUS);
}

static void ui_draw_chart_full(const loader_runtime_t *rt)
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
                       (double)rt->power_measurement, (double)s_peak_p);
    } else {
        ymax = UI_I_MAX;
        color = UI_COL_AMBER;
        ch = "I";
        (void)snprintf(buf, sizeof(buf), "I 5s %s  %.3fA pk%.2f",
                       s_chart.paused ? "HOLD" : "RUN",
                       (double)rt->current_measurement, (double)s_peak_i);
    }
    ui_text(2, 2, 1, UI_COL_CYAN, UI_COL_BG, buf);

    ui_chart_draw(2, 14, 157, 108, ymin, ymax, color);

    (void)snprintf(buf, sizeof(buf), "Y:0..%.0f%s hold:page click:I/P",
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
    ui_text(4, 56, 1, UI_COL_DIM, UI_COL_BG, "UI     v0.2 runtime");
    ui_text(4, 68, 1, UI_COL_DIM, UI_COL_BG, "Keys   PC8/14/7/6");
    ui_text(4, 80, 1, UI_COL_DIM, UI_COL_BG, "TFT    ST7735 160x128");
    ui_text(4, 98, 1, UI_COL_GREEN, UI_COL_BG, "Runtime linked");
    ui_draw_tab_bar(UI_PAGE_ABOUT);
}

static void ui_draw_page(const loader_runtime_t *rt)
{
    switch (s_page) {
    case UI_PAGE_HOME:   ui_draw_home(rt);       break;
    case UI_PAGE_SET:    ui_draw_set(rt);        break;
    case UI_PAGE_STATUS: ui_draw_status(rt);     break;
    case UI_PAGE_CHART:  ui_draw_chart_full(rt); break;
    case UI_PAGE_ABOUT:  ui_draw_about();        break;
    default:             ui_draw_home(rt);       break;
    }
    s_last_drawn = s_page;
}

/* -------------------------------------------------------------------------- */
/* 输入处理（LOADER_UI.md §3）                                                  */
/* -------------------------------------------------------------------------- */

static void ui_page_set(ui_page_t page)
{
    s_page = page;
    s_editing = false;
}

static void ui_page_delta(int delta)
{
    int p = (int)s_page + delta;
    while (p < 0) {
        p += (int)UI_PAGE_COUNT;
    }
    s_page = (ui_page_t)(p % (int)UI_PAGE_COUNT);
    s_editing = false;
}

static void ui_handle_key(ui_key_t key)
{
    loader_runtime_t rt = loader_runtime_get();
    const bool is_up   = (key == UI_KEY_UP || key == UI_KEY_UP_LONG);
    const bool is_down = (key == UI_KEY_DOWN || key == UI_KEY_DOWN_LONG);
    const bool is_long = (key == UI_KEY_UP_LONG || key == UI_KEY_DOWN_LONG);

    switch (s_page) {
    case UI_PAGE_HOME:
        /* 短按 / 长按均可翻页（长按不连发，避免连跳） */
        if (key == UI_KEY_UP || key == UI_KEY_UP_LONG) {
            ui_page_delta(+1);
        } else if (key == UI_KEY_DOWN || key == UI_KEY_DOWN_LONG) {
            ui_page_delta(-1);
        } else if (key == UI_KEY_ENT) {
            ui_toggle_output();
        }
        /* BACK 无效（根页） */
        break;

    case UI_PAGE_SET:
        if (s_editing) {
            float step = s_steps[s_step_idx % 3u];
            /* 编辑态：短按与长按都当步进（hold 连发短键） */
            if (is_up) {
                s_edit_value = ui_clampf(s_edit_value + step,
                                         ui_setpoint_min(rt.mode),
                                         ui_setpoint_max(rt.mode));
            } else if (is_down) {
                s_edit_value = ui_clampf(s_edit_value - step,
                                         ui_setpoint_min(rt.mode),
                                         ui_setpoint_max(rt.mode));
            } else if (key == UI_KEY_ENT) {
                ui_write_active_setpoint(rt.mode, s_edit_value);
                s_editing = false;
            } else if (key == UI_KEY_BACK) {
                s_edit_value = s_edit_backup;
                s_editing = false;
            }
            break;
        }

        /* 非编辑：长按上下切页签 */
        if (is_long) {
            if (key == UI_KEY_UP_LONG) {
                ui_page_delta(+1);
            } else {
                ui_page_delta(-1);
            }
            break;
        }

        if (key == UI_KEY_UP) {
            s_set_sel = (uint8_t)((s_set_sel + 4u) % 5u); /* 上一项 */
        } else if (key == UI_KEY_DOWN) {
            s_set_sel = (uint8_t)((s_set_sel + 1u) % 5u);
        } else if (key == UI_KEY_ENT) {
            switch (s_set_sel) {
            case 0:
                ui_cycle_mode();
                break;
            case 1:
                s_edit_backup = ui_active_setpoint(&rt);
                s_edit_value = s_edit_backup;
                s_editing = true;
                break;
            case 2:
                ui_toggle_output();
                break;
            case 3:
                s_step_idx = (uint8_t)((s_step_idx + 1u) % 3u);
                break;
            case 4:
                ui_page_set(UI_PAGE_HOME);
                break;
            default:
                break;
            }
        } else if (key == UI_KEY_BACK) {
            ui_page_set(UI_PAGE_HOME);
        }
        break;

    case UI_PAGE_STATUS:
        if (key == UI_KEY_ENT && rt.state == LOADER_STATE_ERROR) {
            (void)loader_core_clear_fault();
        } else if (key == UI_KEY_BACK) {
            ui_page_set(UI_PAGE_HOME);
        } else if (key == UI_KEY_UP || key == UI_KEY_UP_LONG) {
            ui_page_delta(+1);
        } else if (key == UI_KEY_DOWN || key == UI_KEY_DOWN_LONG) {
            ui_page_delta(-1);
        }
        break;

    case UI_PAGE_CHART:
        /* 长按上下切页；短按切 I/P */
        if (is_long) {
            if (key == UI_KEY_UP_LONG) {
                ui_page_delta(+1);
            } else {
                ui_page_delta(-1);
            }
        } else if (key == UI_KEY_UP || key == UI_KEY_DOWN) {
            s_chart.show_power = !s_chart.show_power;
            /* 切换通道后清空，避免量纲混画 */
            ui_chart_init();
        } else if (key == UI_KEY_ENT) {
            s_chart.paused = !s_chart.paused;
        } else if (key == UI_KEY_BACK) {
            ui_page_set(UI_PAGE_HOME);
        }
        break;

    case UI_PAGE_ABOUT:
        if (key == UI_KEY_BACK || key == UI_KEY_ENT) {
            ui_page_set(UI_PAGE_HOME);
        } else if (key == UI_KEY_UP || key == UI_KEY_UP_LONG) {
            ui_page_delta(+1);
        } else if (key == UI_KEY_DOWN || key == UI_KEY_DOWN_LONG) {
            ui_page_delta(-1);
        }
        break;

    default:
        break;
    }

    (void)s_last_drawn;
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

    ui_chart_init();
    s_page = UI_PAGE_HOME;
    s_set_sel = 0;
    s_step_idx = 1; /* 0.1 */
    s_editing = false;
    s_peak_i = 0.0f;
    s_peak_p = 0.0f;
    s_last_drawn = (ui_page_t)0xFF;

    ui_buttons_init();

    {
        loader_runtime_t rt = loader_runtime_get();
        ui_fill(UI_COL_BG);
        ui_draw_page(&rt);
        mjc_hal_present();
    }

    s_inited = true;
    sys_log_text(info, "loader_ui init OK (runtime+keys) %ux%u",
                 (unsigned)mjc_hal_screen_width(),
                 (unsigned)mjc_hal_screen_height());
}

void loader_ui_poll(void)
{
    loader_runtime_t rt;
    ui_key_t key;

    if (!s_inited) {
        loader_ui_init();
        if (!s_inited) {
            return;
        }
    }

    /* 保护巡检在中频 state 任务；此处只做人机 */
    while (ui_key_pop(&key)) {
        ui_handle_key(key);
    }

    rt = loader_runtime_get();

    if (rt.current_measurement > s_peak_i) {
        s_peak_i = rt.current_measurement;
    } else {
        s_peak_i *= 0.999f;
    }
    if (rt.power_measurement > s_peak_p) {
        s_peak_p = rt.power_measurement;
    } else {
        s_peak_p *= 0.999f;
    }

    if (!s_chart.paused) {
        if (s_page == UI_PAGE_CHART && s_chart.show_power) {
            ui_chart_push(rt.power_measurement);
        } else {
            ui_chart_push(rt.current_measurement);
        }
    }

    ui_draw_page(&rt);
    mjc_hal_present();
}
