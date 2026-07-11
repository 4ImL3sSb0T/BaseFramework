/**
 * @file mjc_demo.c
 * @brief MujicaUI menu demo on ST7735 (tft_fb + Adafruit GFX).
 *
 * KEY (PC13, active low):
 *   single click  → next item
 *   double click  → enter / toggle / action
 *   long press    → back to parent menu
 */

#include "mjc_core.h"
#include "mjc_define.h"
#include "mjc_render.h"
#include "multi_button.h"
#include "main.h"
#include "cmsis_os.h"
#include "rtt.h"

#include <stddef.h>
#include <stdint.h>

/* ── Demo state ───────────────────────────────────────────────────── */

static uint8_t s_led_enable = 1;
static uint8_t s_brightness = 50;
static uint8_t s_volume = 30;
static uint8_t s_demo_flag = 0;

/* ── Forward ──────────────────────────────────────────────────────── */

static void action_about(mjc_item_t *item, mjc_event_type_t event, void *user_data);
static void action_reset_defaults(mjc_item_t *item, mjc_event_type_t event, void *user_data);

/* ── Sub page: Settings ───────────────────────────────────────────── */

static mjc_item_t s_settings_items[] = {
    {
        .name = "Brightness",
        .type = MJC_ITEM_TYPE_NUMBER,
        .data.number = {
            .value_ptr = &s_brightness,
            .num_type = MJC_NUM_UINT8,
            .step = 5.0f,
            .min = 0.0f,
            .max = 100.0f,
        },
        .action = NULL,
        .user_data = NULL,
    },
    {
        .name = "Volume",
        .type = MJC_ITEM_TYPE_NUMBER,
        .data.number = {
            .value_ptr = &s_volume,
            .num_type = MJC_NUM_UINT8,
            .step = 5.0f,
            .min = 0.0f,
            .max = 100.0f,
        },
        .action = NULL,
        .user_data = NULL,
    },
    {
        .name = "Reset Def",
        .type = MJC_ITEM_TYPE_ACTION,
        .data = {0},
        .action = action_reset_defaults,
        .user_data = NULL,
    },
};

static mjc_page_t s_settings_page = {
    .items = s_settings_items,
    .count = (uint8_t)(sizeof(s_settings_items) / sizeof(s_settings_items[0])),
    .selected_index = 0,
    .parent_page = NULL,
};

/* ── Root page ────────────────────────────────────────────────────── */

static mjc_item_t s_root_items[] = {
    {
        .name = "H750 Menu",
        .type = MJC_ITEM_TYPE_LABEL,
        .data = {0},
        .action = NULL,
        .user_data = NULL,
    },
    {
        .name = "LED Enable",
        .type = MJC_ITEM_TYPE_CHECKBOX,
        .data.checkbox = &s_led_enable,
        .action = NULL,
        .user_data = NULL,
    },
    {
        .name = "Settings",
        .type = MJC_ITEM_TYPE_SUBMENU,
        .data.submenu = &s_settings_page,
        .action = NULL,
        .user_data = NULL,
    },
    {
        .name = "Demo Flag",
        .type = MJC_ITEM_TYPE_CHECKBOX,
        .data.checkbox = &s_demo_flag,
        .action = NULL,
        .user_data = NULL,
    },
    {
        .name = "About",
        .type = MJC_ITEM_TYPE_ACTION,
        .data = {0},
        .action = action_about,
        .user_data = NULL,
    },
};

static mjc_page_t s_root_page = {
    .items = s_root_items,
    .count = (uint8_t)(sizeof(s_root_items) / sizeof(s_root_items[0])),
    .selected_index = 1, /* first selectable after title */
    .parent_page = NULL,
};

/* ── Actions ──────────────────────────────────────────────────────── */

static void action_about(mjc_item_t *item, mjc_event_type_t event, void *user_data)
{
    (void)item;
    (void)user_data;
    if (event == MJC_EVENT_TRIGGER) {
        log_rtt_println("[MJC] About: MujicaUI + ST7735 GFX");
    }
}

static void action_reset_defaults(mjc_item_t *item, mjc_event_type_t event, void *user_data)
{
    (void)item;
    (void)user_data;
    if (event == MJC_EVENT_TRIGGER) {
        s_brightness = 50;
        s_volume = 30;
        s_led_enable = 1;
        s_demo_flag = 0;
        log_rtt_println("[MJC] Defaults restored");
    }
}

/* Number item: +/- on trigger while focused is not ideal with one key.
 * Map trigger on NUMBER as step up, wrap at max. */
static void number_step_action(mjc_item_t *item, mjc_event_type_t event, void *user_data)
{
    (void)user_data;
    if (item == NULL || item->type != MJC_ITEM_TYPE_NUMBER || event != MJC_EVENT_TRIGGER) {
        return;
    }
    mjc_number_config_t *n = &item->data.number;
    if (n->value_ptr == NULL || n->num_type != MJC_NUM_UINT8) {
        return;
    }
    uint8_t *v = (uint8_t *)n->value_ptr;
    float next = (float)(*v) + n->step;
    if (next > n->max) {
        next = n->min;
    }
    *v = (uint8_t)next;
}

/* ── Button ───────────────────────────────────────────────────────── */

static Button s_key;
static volatile uint8_t s_need_redraw = 1;

static uint8_t key_read_level(uint8_t button_id)
{
    (void)button_id;
    /* USR_KEY: pressed = GPIO_PIN_RESET (active low) */
    return (HAL_GPIO_ReadPin(USR_KEY_GPIO_Port, USR_KEY_Pin) == GPIO_PIN_RESET) ? 1 : 0;
}

static void on_single_click(Button *btn)
{
    (void)btn;
    mjc_page_down();
    s_need_redraw = 1;
}

static void on_double_click(Button *btn)
{
    (void)btn;
    mjc_item_t *it = mjc_get_selected_item();
    if (it != NULL) {
        mjc_item_execute(it, MJC_EVENT_TRIGGER);
    }
    s_need_redraw = 1;
}

static void on_long_press(Button *btn)
{
    (void)btn;
    mjc_page_back();
    s_need_redraw = 1;
}

static void mjc_demo_bind_number_actions(void)
{
    for (uint8_t i = 0; i < s_settings_page.count; i++) {
        if (s_settings_items[i].type == MJC_ITEM_TYPE_NUMBER) {
            s_settings_items[i].action = number_step_action;
        }
    }
}

/* ── Public entry ─────────────────────────────────────────────────── */

void mjc_demo_task(void)
{
    log_rtt_println("=== MujicaUI menu demo ===");

    mjc_demo_bind_number_actions();

    if (!mjc_init(&s_root_page)) {
        log_rtt_println("mjc_init FAIL");
        return;
    }

    button_init(&s_key, key_read_level, 1 /* active high after map */, 0);
    button_attach(&s_key, BTN_SINGLE_CLICK, on_single_click);
    button_attach(&s_key, BTN_DOUBLE_CLICK, on_double_click);
    button_attach(&s_key, BTN_LONG_PRESS_START, on_long_press);
    button_start(&s_key);

    s_need_redraw = 1;
    mjc_update();
    log_rtt_println("Menu ready: KEY=nav, 2x=enter, long=back");

    for (;;) {
        button_ticks();

        if (s_need_redraw) {
            s_need_redraw = 0;
            mjc_update();
        }

        if (s_led_enable) {
            HAL_GPIO_WritePin(GREEN_GPIO_Port, GREEN_Pin, GPIO_PIN_SET);
        } else {
            HAL_GPIO_WritePin(GREEN_GPIO_Port, GREEN_Pin, GPIO_PIN_RESET);
        }

        osDelay(TICKS_INTERVAL);
    }
}
