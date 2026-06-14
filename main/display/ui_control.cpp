/* ================================================================
 *  ui_control.cpp - 色温/亮度控制页 (页面1)
 *  左侧: 垂直亮度滑块 + 右侧: 色温渐变条 + 底部预设按钮
 * ================================================================ */

#include "ui_control.h"
#include "ui_theme.h"
#include "config.h"
#include "led_control.h"
#include <stdio.h>

static lv_obj_t *slider_brt  = nullptr;
static lv_obj_t *lbl_brt_val = nullptr;
static lv_obj_t *lbl_brt_pct = nullptr;
static lv_obj_t *bar_cct     = nullptr;
static lv_obj_t *lbl_cct_val = nullptr;

/* 色温渐变色条: 用多个小色块模拟渐变 */
#define GRADIENT_STEPS  20
static lv_obj_t *grad_blocks[GRADIENT_STEPS] = {};

static void create_cct_gradient(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                 lv_coord_t w, lv_coord_t h)
{
    lv_coord_t bw = w / GRADIENT_STEPS;
    for (int i = 0; i < GRADIENT_STEPS; i++) {
        uint16_t cct = 2700 + (uint32_t)i * (6500 - 2700) / (GRADIENT_STEPS - 1);
        lv_color_t col = cct_to_color(cct);

        grad_blocks[i] = lv_obj_create(parent);
        lv_obj_set_size(grad_blocks[i], bw, h);
        lv_obj_set_pos(grad_blocks[i], x + i * bw, y);
        lv_obj_set_style_bg_color(grad_blocks[i], col, 0);
        lv_obj_set_style_bg_opa(grad_blocks[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(grad_blocks[i], 0, 0);
        lv_obj_set_style_radius(grad_blocks[i], 0, 0);
        lv_obj_clear_flag(grad_blocks[i], LV_OBJ_FLAG_SCROLLABLE);
    }
}

/* 预设按钮点击回调 */
static void preset_btn_cb(lv_event_t *e)
{
    uint16_t cct = (uint16_t)(uintptr_t)lv_event_get_user_data(e);
    g_colorTemp = cct;
    led_set_cct(cct);
    control_update_brt_cct(g_brightness, cct);
}

static lv_obj_t* make_preset_btn(lv_obj_t *parent, const char *text,
                                  uint16_t cct, lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 50, 32);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, C_CARD, 0);
    lv_obj_set_style_bg_color(btn, C_CYAN, LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, 6, 0);
    lv_obj_set_style_border_color(btn, C_CARD_BORDER, 0);
    lv_obj_set_style_border_width(btn, 1, 0);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    style_label(lbl, C_WHITE, FONT_SMALL);
    lv_obj_center(lbl);

    lv_obj_add_event_cb(btn, preset_btn_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)cct);
    return btn;
}

void control_create(lv_obj_t *parent)
{
    lv_coord_t pad = 16;

    /* ---- 亮度标签 ---- */
    lv_obj_t *t_brt = lv_label_create(parent);
    lv_label_set_text(t_brt, "BRT");
    style_label(t_brt, C_CYAN, FONT_MEDIUM);
    lv_obj_set_pos(t_brt, pad, 12);

    lbl_brt_pct = lv_label_create(parent);
    lv_label_set_text(lbl_brt_pct, "0%");
    style_label(lbl_brt_pct, C_WHITE, FONT_LARGE);
    lv_obj_set_pos(lbl_brt_pct, pad, 32);

    /* ---- 垂直亮度滑块 ---- */
    slider_brt = lv_slider_create(parent);
    lv_obj_set_size(slider_brt, 16, 120);
    lv_obj_set_pos(slider_brt, pad + 8, 60);
    lv_slider_set_range(slider_brt, 0, 100);
    lv_slider_set_value(slider_brt, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider_brt, C_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider_brt, C_CYAN, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider_brt, C_WHITE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider_brt, 4, LV_PART_KNOB);
    lv_obj_set_style_radius(slider_brt, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(slider_brt, 4, LV_PART_INDICATOR);

    /* ---- 色温区域 ---- */
    lv_obj_t *t_cct = lv_label_create(parent);
    lv_label_set_text(t_cct, "CCT");
    style_label(t_cct, C_ORANGE, FONT_MEDIUM);
    lv_obj_set_pos(t_cct, 80, 12);

    lbl_cct_val = lv_label_create(parent);
    lv_label_set_text(lbl_cct_val, "4600K");
    style_label(lbl_cct_val, C_WHITE, FONT_LARGE);
    lv_obj_set_pos(lbl_cct_val, 80, 32);

    /* ---- 色温渐变条 ---- */
    create_cct_gradient(parent, 80, 60, 140, 24);

    /* 色温指示器 (当前位置标记) */
    bar_cct = lv_obj_create(parent);
    lv_obj_set_size(bar_cct, 4, 28);
    lv_obj_set_pos(bar_cct, 80, 58);
    lv_obj_set_style_bg_color(bar_cct, C_WHITE, 0);
    lv_obj_set_style_bg_opa(bar_cct, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bar_cct, 2, 0);
    lv_obj_set_style_border_width(bar_cct, 0, 0);
    lv_obj_clear_flag(bar_cct, LV_OBJ_FLAG_SCROLLABLE);

    /* 端点标签 */
    lv_obj_t *t_lo = lv_label_create(parent);
    lv_label_set_text(t_lo, "2700K");
    style_label(t_lo, C_DIM, FONT_SMALL);
    lv_obj_set_pos(t_lo, 80, 88);

    lv_obj_t *t_hi = lv_label_create(parent);
    lv_label_set_text(t_hi, "6500K");
    style_label(t_hi, C_DIM, FONT_SMALL);
    lv_obj_set_pos(t_hi, 175, 88);

    /* ---- 预设按钮 ---- */
    lv_coord_t by = 115;
    lv_coord_t bx = 80;
    make_preset_btn(parent, "27K", 2700, bx, by);
    make_preset_btn(parent, "35K", 3500, bx + 56, by);
    make_preset_btn(parent, "46K", 4600, bx + 112, by);
    make_preset_btn(parent, "65K", 6500, bx, by + 40);
}

void control_update_brt_cct(uint8_t brt, uint16_t cct)
{
    char buf[16];

    if (slider_brt) lv_slider_set_value(slider_brt, brt, LV_ANIM_ON);
    if (lbl_brt_pct) {
        snprintf(buf, sizeof(buf), "%d%%", brt);
        lv_label_set_text(lbl_brt_pct, buf);
    }

    if (lbl_cct_val) {
        snprintf(buf, sizeof(buf), "%uK", cct);
        lv_label_set_text(lbl_cct_val, buf);
        lv_color_t col = cct_to_color(cct);
        lv_obj_set_style_text_color(lbl_cct_val, col, 0);
    }

    /* 色温指示器位置 */
    if (bar_cct) {
        lv_coord_t x = 80 + (lv_coord_t)((uint32_t)(cct - 2700) * 136 / (6500 - 2700));
        lv_obj_set_x(bar_cct, x);
    }
}
