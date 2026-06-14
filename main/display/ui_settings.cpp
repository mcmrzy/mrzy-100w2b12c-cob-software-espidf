/* ================================================================
 *  ui_settings.cpp - 系统设置页 (页面3)
 *  WiFi/BLE/OTA/场景/风扇 设置项列表
 * ================================================================ */

#include "ui_settings.h"
#include "ui_theme.h"
#include <stdio.h>

#define ROW_H       36
#define ROW_GAP     4
#define LIST_X      12
#define LIST_W      (SCREEN_W - 24)

static lv_obj_t *rows[6] = {};
static lv_obj_t *vals[6] = {};

static const char *row_labels[] = {
    "WiFi", "BLE", "OTA", "Scene", "FAN", "Version"
};

static const char *row_defaults[] = {
    "AP", "ON", "RDY", "---", "AUTO", "v1.0"
};

static void create_setting_row(lv_obj_t *parent, int idx, lv_coord_t y)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, LIST_W, ROW_H);
    lv_obj_set_pos(row, LIST_X, y);
    style_card(row);
    lv_obj_set_style_pad_hor(row, 10, 0);

    /* 标签 */
    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text(lbl, row_labels[idx]);
    style_label(lbl, C_WHITE, FONT_NORMAL);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);

    /* 状态值 */
    lv_obj_t *val = lv_label_create(row);
    lv_label_set_text(val, row_defaults[idx]);
    style_label(val, C_CYAN, FONT_NORMAL);
    lv_obj_align(val, LV_ALIGN_RIGHT_MID, -12, 0);

    /* 箭头 */
    lv_obj_t *arr = lv_label_create(row);
    lv_label_set_text(arr, LV_SYMBOL_RIGHT);
    style_label(arr, C_DIM, FONT_NORMAL);
    lv_obj_align(arr, LV_ALIGN_RIGHT_MID, 0, 0);

    rows[idx] = row;
    vals[idx] = val;
}

void settings_create(lv_obj_t *parent)
{
    lv_coord_t y = 8;
    for (int i = 0; i < 6; i++) {
        create_setting_row(parent, i, y);
        y += ROW_H + ROW_GAP;
    }

    /* 保存按钮 */
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, LIST_W, 36);
    lv_obj_set_pos(btn, LIST_X, y + 8);
    lv_obj_set_style_bg_color(btn, C_CYAN, 0);
    lv_obj_set_style_bg_color(btn, C_BLUE, LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, 8, 0);

    lv_obj_t *bl = lv_label_create(btn);
    lv_label_set_text(bl, LV_SYMBOL_OK "  Save Settings");
    style_label(bl, lv_color_hex(0x0A0A12), FONT_NORMAL);
    lv_obj_center(bl);
}
