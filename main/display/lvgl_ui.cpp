#include "lvgl_ui.h"
#include "led_control.h"
#include <stdio.h>
#include <string.h>

/* ========== Color Theme ========== */
#define COLOR_BG           0x0E0E14
#define COLOR_CARD         0x1A1A24
#define COLOR_ACCENT       0x00D4AA
#define COLOR_WARM         0xFF8C42
#define COLOR_COLD         0x5CACEE
#define COLOR_TEXT         0xEAEAEA
#define COLOR_TEXT_DIM     0x707088
#define COLOR_DANGER       0xFF5555
#define COLOR_WARNING      0xFFB86C
#define COLOR_SUCCESS      0x50FA7B
#define COLOR_GLOW_WARM    0xFF6B35
#define COLOR_GLOW_COLD    0x4DA6FF

/* ========== Static Widgets ========== */
static lv_obj_t *scr = NULL;

/* Top Bar */
static lv_obj_t *lbl_env = NULL;

/* Center Arc */
static lv_obj_t *arc_main = NULL;
static lv_obj_t *lbl_cct_val = NULL;
static lv_obj_t *lbl_brt_val = NULL;

/* Slider */
static lv_obj_t *slider_cct = NULL;

/* Data Cards */
static lv_obj_t *lbl_bus_v = NULL;
static lv_obj_t *lbl_bus_i = NULL;
static lv_obj_t *lbl_bus_p = NULL;
static lv_obj_t *lbl_bat_v = NULL;
static lv_obj_t *lbl_bat_i = NULL;
static lv_obj_t *lbl_bat_p = NULL;
static lv_obj_t *bar_brightness = NULL;
static lv_obj_t *lbl_brightness_pct = NULL;

/* Scene Buttons */
static lv_obj_t *btn_scene[4] = {NULL};

/* Bottom Bar */
static lv_obj_t *lbl_temp_led = NULL;
static lv_obj_t *lbl_temp_env = NULL;
static lv_obj_t *lbl_pressure = NULL;

/* Scene Data */
static const char *scene_names[4] = {"Warm", "Neutral", "Cool", "Turbo"};
static const uint16_t scene_cct[4] = {2700, 4000, 5500, 6500};
static const uint8_t  scene_brt[4] = {40, 70, 85, 100};

/* ========== Helpers ========== */
static lv_color_t mix_color(lv_color_t c1, lv_color_t c2, uint8_t ratio) {
    uint8_t r = (uint8_t)(((uint16_t)lv_color_red(c1) * (255 - ratio) + (uint16_t)lv_color_red(c2) * ratio) / 255);
    uint8_t g = (uint8_t)(((uint16_t)lv_color_green(c1) * (255 - ratio) + (uint16_t)lv_color_green(c2) * ratio) / 255);
    uint8_t b = (uint8_t)(((uint16_t)lv_color_blue(c1) * (255 - ratio) + (uint16_t)lv_color_blue(c2) * ratio) / 255);
    return lv_color_make(r, g, b);
}

static lv_color_t get_cct_color(uint16_t cct) {
    if (cct <= 2700) return lv_color_hex(COLOR_WARM);
    if (cct >= 6500) return lv_color_hex(COLOR_COLD);
    uint8_t ratio = (uint8_t)(((cct - 2700) * 255) / (6500 - 2700));
    return mix_color(lv_color_hex(COLOR_WARM), lv_color_hex(COLOR_COLD), ratio);
}

static lv_color_t get_glow_color(uint16_t cct) {
    if (cct <= 2700) return lv_color_hex(COLOR_GLOW_WARM);
    if (cct >= 6500) return lv_color_hex(COLOR_GLOW_COLD);
    uint8_t ratio = (uint8_t)(((cct - 2700) * 255) / (6500 - 2700));
    return mix_color(lv_color_hex(COLOR_GLOW_WARM), lv_color_hex(COLOR_GLOW_COLD), ratio);
}

static lv_obj_t* make_data_card(lv_obj_t *parent, int x, int y, int w, int h,
                                 const char *title, const char *unit) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(COLOR_CARD), 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    /* Value label */
    lv_obj_t *lbl_val = lv_label_create(card);
    lv_label_set_text(lbl_val, "--");
    lv_obj_set_style_text_color(lbl_val, lv_color_hex(COLOR_TEXT), 0);
    lv_obj_set_style_text_font(lbl_val, &lv_font_montserrat_16, 0);
    lv_obj_align(lbl_val, LV_ALIGN_TOP_MID, 0, 4);

    /* Unit */
    lv_obj_t *lbl_unit = lv_label_create(card);
    lv_label_set_text(lbl_unit, unit);
    lv_obj_set_style_text_color(lbl_unit, lv_color_hex(COLOR_TEXT_DIM), 0);
    lv_obj_set_style_text_font(lbl_unit, &lv_font_montserrat_10, 0);
    lv_obj_align(lbl_unit, LV_ALIGN_TOP_MID, 0, 24);

    /* Title */
    lv_obj_t *lbl_title = lv_label_create(card);
    lv_label_set_text(lbl_title, title);
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(COLOR_TEXT_DIM), 0);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_9, 0);
    lv_obj_align(lbl_title, LV_ALIGN_BOTTOM_MID, 0, -3);

    return lbl_val;
}

/* ========== Events ========== */
static void slider_event_cb(lv_event_t *e) {
    int16_t v = (int16_t)lv_slider_get_value(slider_cct);
    uint16_t cct = (uint16_t)(2700 + (uint32_t)v * (6500 - 2700) / 100);
    lv_color_t col = get_cct_color(cct);

    char buf[16];
    snprintf(buf, sizeof(buf), "%dK", cct);
    lv_label_set_text(lbl_cct_val, buf);
    lv_obj_set_style_text_color(lbl_cct_val, col, 0);

    lv_obj_set_style_arc_color(arc_main, col, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(arc_main, col, LV_PART_KNOB);

    led_set_cct(cct);
}

static void scene_event_cb(lv_event_t *e) {
    uint8_t idx = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    led_set_cct(scene_cct[idx]);
    led_set_brightness(scene_brt[idx]);

    int16_t sv = (int16_t)((uint32_t)(scene_cct[idx] - 2700) * 100 / (6500 - 2700));
    lv_slider_set_value(slider_cct, sv, LV_ANIM_ON);

    char buf[16];
    snprintf(buf, sizeof(buf), "%dK", scene_cct[idx]);
    lv_label_set_text(lbl_cct_val, buf);

    snprintf(buf, sizeof(buf), "%d%%", scene_brt[idx]);
    lv_label_set_text(lbl_brt_val, buf);
    lv_bar_set_value(bar_brightness, scene_brt[idx], LV_ANIM_ON);
    snprintf(buf, sizeof(buf), "%d%%", scene_brt[idx]);
    lv_label_set_text(lbl_brightness_pct, buf);

    lv_color_t col = get_cct_color(scene_cct[idx]);
    lv_obj_set_style_text_color(lbl_cct_val, col, 0);
    lv_obj_set_style_arc_color(arc_main, col, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(arc_main, col, LV_PART_KNOB);

    /* Highlight selected */
    for (int i = 0; i < 4; i++) {
        lv_obj_set_style_bg_color(btn_scene[i], (i == idx) ? col : lv_color_hex(COLOR_CARD), 0);
    }
}

/* ========== lvgl_ui_init() ========== */
void lvgl_ui_init(void) {
    scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* ===== Top Bar ===== */
    lbl_env = lv_label_create(scr);
    lv_label_set_text(lbl_env, "28°C  ·  1013hPa");
    lv_obj_set_style_text_color(lbl_env, lv_color_hex(COLOR_TEXT_DIM), 0);
    lv_obj_set_style_text_font(lbl_env, &lv_font_montserrat_10, 0);
    lv_obj_align(lbl_env, LV_ALIGN_TOP_MID, 0, 6);

    /* ===== Center Arc ===== */
    arc_main = lv_arc_create(scr);
    lv_arc_set_range(arc_main, 0, 100);
    lv_arc_set_value(arc_main, 50);
    lv_obj_set_size(arc_main, 140, 140);
    lv_obj_align(arc_main, LV_ALIGN_TOP_MID, 0, 28);
    lv_obj_set_style_arc_width(arc_main, 10, 0);
    lv_obj_set_style_arc_color(arc_main, lv_color_hex(COLOR_CARD), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_main, 10, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc_main, lv_color_hex(COLOR_WARM), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(arc_main, LV_OPA_TRANSP, 0);
    lv_obj_set_style_outline_width(arc_main, 0, 0);
    lv_obj_set_style_pad_all(arc_main, 0, LV_PART_KNOB);
    lv_obj_set_style_bg_color(arc_main, lv_color_hex(COLOR_WARM), LV_PART_KNOB);
    lv_obj_clear_flag(arc_main, LV_OBJ_FLAG_CLICKABLE);

    /* Center text */
    lbl_cct_val = lv_label_create(scr);
    lv_label_set_text(lbl_cct_val, "4600K");
    lv_obj_set_style_text_color(lbl_cct_val, lv_color_hex(COLOR_WARM), 0);
    lv_obj_set_style_text_font(lbl_cct_val, &lv_font_montserrat_20, 0);
    lv_obj_align(lbl_cct_val, LV_ALIGN_TOP_MID, 0, 60);

    lbl_brt_val = lv_label_create(scr);
    lv_label_set_text(lbl_brt_val, "75%");
    lv_obj_set_style_text_color(lbl_brt_val, lv_color_hex(COLOR_TEXT_DIM), 0);
    lv_obj_set_style_text_font(lbl_brt_val, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_brt_val, LV_ALIGN_TOP_MID, 0, 82);

    /* ===== Brightness Bar ===== */
    lv_obj_t *lbl_brt_icon = lv_label_create(scr);
    lv_label_set_text(lbl_brt_icon, "💡");
    lv_obj_set_style_text_font(lbl_brt_icon, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(lbl_brt_icon, 16, 177);

    bar_brightness = lv_bar_create(scr);
    lv_obj_set_size(bar_brightness, 150, 12);
    lv_obj_set_pos(bar_brightness, 42, 178);
    lv_bar_set_range(bar_brightness, 0, 100);
    lv_bar_set_value(bar_brightness, 75, LV_ANIM_OFF);
    lv_obj_set_style_radius(bar_brightness, 6, 0);
    lv_obj_set_style_bg_color(bar_brightness, lv_color_hex(COLOR_CARD), 0);
    lv_obj_set_style_radius(bar_brightness, 6, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(bar_brightness, lv_color_hex(COLOR_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_border_width(bar_brightness, 0, 0);

    lbl_brightness_pct = lv_label_create(scr);
    lv_label_set_text(lbl_brightness_pct, "75%");
    lv_obj_set_style_text_color(lbl_brightness_pct, lv_color_hex(COLOR_TEXT), 0);
    lv_obj_set_style_text_font(lbl_brightness_pct, &lv_font_montserrat_11, 0);
    lv_obj_align(lbl_brightness_pct, LV_ALIGN_TOP_RIGHT, -16, 176);

    /* ===== CCT Slider ===== */
    slider_cct = lv_slider_create(scr);
    lv_obj_set_size(slider_cct, 208, 16);
    lv_obj_set_pos(slider_cct, 16, 196);
    lv_slider_set_range(slider_cct, 0, 100);
    lv_slider_set_value(slider_cct, 50, LV_ANIM_OFF);
    lv_obj_set_style_radius(slider_cct, 8, 0);
    lv_obj_set_style_bg_color(slider_cct, lv_color_hex(COLOR_CARD), 0);
    lv_obj_set_style_bg_color(slider_cct, lv_color_hex(COLOR_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider_cct, lv_color_hex(COLOR_TEXT), LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider_cct, 0, 0);
    lv_obj_set_style_border_width(slider_cct, 0, 0);
    lv_obj_clear_flag(slider_cct, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(slider_cct, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* Slider labels */
    lv_obj_t *lbl_w = lv_label_create(scr);
    lv_label_set_text(lbl_w, "2700K");
    lv_obj_set_style_text_color(lbl_w, lv_color_hex(COLOR_TEXT_DIM), 0);
    lv_obj_set_style_text_font(lbl_w, &lv_font_montserrat_9, 0);
    lv_obj_set_pos(lbl_w, 16, 210);

    lv_obj_t *lbl_c = lv_label_create(scr);
    lv_label_set_text(lbl_c, "6500K");
    lv_obj_set_style_text_color(lbl_c, lv_color_hex(COLOR_TEXT_DIM), 0);
    lv_obj_set_style_text_font(lbl_c, &lv_font_montserrat_9, 0);
    lv_obj_align(lbl_c, LV_ALIGN_TOP_RIGHT, -16, 210);

    /* ===== Data Cards (Bus) ===== */
    lbl_bus_v = make_data_card(scr, 16, 228, 64, 42, "BUS V", "V");
    lbl_bus_i = make_data_card(scr, 88, 228, 64, 42, "BUS I", "A");
    lbl_bus_p = make_data_card(scr, 160, 228, 64, 42, "BUS P", "W");

    /* ===== Data Cards (Battery) ===== */
    lbl_bat_v = make_data_card(scr, 16, 278, 64, 42, "BAT V", "V");
    lbl_bat_i = make_data_card(scr, 88, 278, 64, 42, "BAT I", "A");
    lbl_bat_p = make_data_card(scr, 160, 278, 64, 42, "BAT P", "W");

    /* ===== Bottom Status Row ===== */
    lbl_temp_led = lv_label_create(scr);
    lv_label_set_text(lbl_temp_led, "LED:--°C");
    lv_obj_set_style_text_color(lbl_temp_led, lv_color_hex(COLOR_TEXT_DIM), 0);
    lv_obj_set_style_text_font(lbl_temp_led, &lv_font_montserrat_9, 0);
    lv_obj_set_pos(lbl_temp_led, 16, 326);

    lbl_temp_env = lv_label_create(scr);
    lv_label_set_text(lbl_temp_env, "ENV:--°C");
    lv_obj_set_style_text_color(lbl_temp_env, lv_color_hex(COLOR_TEXT_DIM), 0);
    lv_obj_set_style_text_font(lbl_temp_env, &lv_font_montserrat_9, 0);
    lv_obj_align(lbl_temp_env, LV_ALIGN_BOTTOM_MID, 0, 0);

    lbl_pressure = lv_label_create(scr);
    lv_label_set_text(lbl_pressure, "----hPa");
    lv_obj_set_style_text_color(lbl_pressure, lv_color_hex(COLOR_TEXT_DIM), 0);
    lv_obj_set_style_text_font(lbl_pressure, &lv_font_montserrat_9, 0);
    lv_obj_align(lbl_pressure, LV_ALIGN_BOTTOM_RIGHT, -16, 0);
}

/* ========== Update Functions ========== */
void lvgl_ui_update_cct(uint16_t cct) {
    if (!slider_cct || !lbl_cct_val) return;
    int16_t v = (int16_t)((uint32_t)(cct - 2700) * 100 / (6500 - 2700));
    if (v < 0) v = 0; if (v > 100) v = 100;
    lv_slider_set_value(slider_cct, v, LV_ANIM_ON);

    lv_color_t col = get_cct_color(cct);
    char buf[16];
    snprintf(buf, sizeof(buf), "%dK", cct);
    lv_label_set_text(lbl_cct_val, buf);
    lv_obj_set_style_text_color(lbl_cct_val, col, 0);
    lv_obj_set_style_arc_color(arc_main, col, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(arc_main, col, LV_PART_KNOB);
}

void lvgl_ui_update_brightness(uint8_t brt) {
    if (!bar_brightness || !lbl_brightness_pct) return;
    lv_bar_set_value(bar_brightness, brt, LV_ANIM_ON);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", brt);
    lv_label_set_text(lbl_brightness_pct, buf);
    snprintf(buf, sizeof(buf), "%d%%", brt);
    lv_label_set_text(lbl_brt_val, buf);
}

void lvgl_ui_update_temp(float temp) {
    if (!lbl_temp_led) return;
    char buf[24];
    snprintf(buf, sizeof(buf), "LED:%.0f°C", temp);
    lv_label_set_text(lbl_temp_led, buf);
}

void lvgl_ui_update_env(float temp, float pressure) {
    if (!lbl_temp_env || !lbl_pressure || !lbl_env) return;
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f°C  ·  %.0fhPa", temp, pressure);
    lv_label_set_text(lbl_env, buf);
    snprintf(buf, sizeof(buf), "ENV:%.0f°C", temp);
    lv_label_set_text(lbl_temp_env, buf);
    snprintf(buf, sizeof(buf), "%.0fhPa", pressure);
    lv_label_set_text(lbl_pressure, buf);
}

void lvgl_ui_update_sensors(float busV, float busI, float busW,
                             float batV, float batI, float batW) {
    if (!lbl_bus_v) return;
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", busV);  lv_label_set_text(lbl_bus_v, buf);
    snprintf(buf, sizeof(buf), "%.2f", busI);  lv_label_set_text(lbl_bus_i, buf);
    snprintf(buf, sizeof(buf), "%.1f", busW);  lv_label_set_text(lbl_bus_p, buf);
    snprintf(buf, sizeof(buf), "%.1f", batV);  lv_label_set_text(lbl_bat_v, buf);
    snprintf(buf, sizeof(buf), "%.2f", batI);  lv_label_set_text(lbl_bat_i, buf);
    snprintf(buf, sizeof(buf), "%.1f", batW);  lv_label_set_text(lbl_bat_p, buf);
}
#include "lvgl_ui.h"
#include "led_control.h"
#include <#include "lvgl_ui.h"
#include "led_control.h"
#include <stdio.h>
#include <string.h>

/* =#include "lvgl_ui.h"
#include "led_control.h"
#include <stdio.h>
#include <string.h>

/* ========== Theme ========== */
#define COLOR_B#include "lvgl_ui.h"
#include "led_control.h"
#include <stdio.h>
#include <string.h>

/* ========== Theme ========== */
#define COLOR_BG           0x0B0B12
#define COLOR_CARD          0x171726
#include "lvgl_ui.h"
#include "led_control.h"
#include <stdio.h>
#include <string.h>

/* ========== Theme ========== */
#define COLOR_BG           0x0B0B12
#define COLOR_CARD          0x171726
#define COLOR_ACCENT        0x00D#include "lvgl_ui.h"
#include "led_control.h"
#include <stdio.h>
#include <string.h>

/* ========== Theme ========== */
#define COLOR_BG           0x0B0B12
#define COLOR_CARD          0x171726
#define COLOR_ACCENT        0x00D4AA
#define COLOR_WARM          0xFF