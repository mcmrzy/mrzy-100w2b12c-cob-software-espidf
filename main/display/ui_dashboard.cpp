/* ================================================================
 *  ui_dashboard.cpp - 仪表盘页 (页面0)
 *  2个圆弧仪表盘 (亮度/色温) + 底部传感器数据卡片
 * ================================================================ */

#include "ui_dashboard.h"
#include "ui_theme.h"
#include <stdio.h>

static lv_obj_t *arc_brt   = nullptr;
static lv_obj_t *arc_cct   = nullptr;
static lv_obj_t *lbl_brt_v = nullptr;
static lv_obj_t *lbl_brt_u = nullptr;
static lv_obj_t *lbl_cct_v = nullptr;
static lv_obj_t *lbl_cct_u = nullptr;

static lv_obj_t *card_v  = nullptr;
static lv_obj_t *card_a  = nullptr;
static lv_obj_t *card_w  = nullptr;
static lv_obj_t *card_lt = nullptr;
static lv_obj_t *card_et = nullptr;

/* 创建一个带标签的数据卡片 */
static lv_obj_t* make_data_card(lv_obj_t *parent, const char *title,
                                 lv_coord_t x, lv_coord_t y, lv_coord_t w)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, w, 44);
    lv_obj_set_pos(card, x, y);
    style_card(card);

    lv_obj_t *t = lv_label_create(card);
    lv_label_set_text(t, title);
    style_label(t, C_DIM, FONT_SMALL);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *v = lv_label_create(card);
    lv_label_set_text(v, "--");
    style_label(v, C_WHITE, FONT_MEDIUM);
    lv_obj_align(v, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_set_user_data(v, (void*)(uintptr_t)1); /* 标记为数值标签 */

    return v;
}

void dashboard_create(lv_obj_t *parent)
{
    /* ---- 亮度圆弧 (上半部分) ---- */
    arc_brt = create_gauge_arc(parent, 120, C_CYAN, 0, 100);
    lv_obj_align(arc_brt, LV_ALIGN_TOP_LEFT, 30, 10);

    /* 亮度数值 (圆弧中心) */
    lbl_brt_v = lv_label_create(parent);
    lv_label_set_text(lbl_brt_v, "0");
    style_label(lbl_brt_v, C_CYAN, FONT_XLARGE);
    lv_obj_align(lbl_brt_v, LV_ALIGN_TOP_LEFT, 30 + 60, 10 + 42);

    lbl_brt_u = lv_label_create(parent);
    lv_label_set_text(lbl_brt_u, "%");
    style_label(lbl_brt_u, C_DIM, FONT_NORMAL);
    lv_obj_align(lbl_brt_u, LV_ALIGN_TOP_LEFT, 30 + 60, 10 + 68);

    /* ---- 色温圆弧 (上半部分右侧) ---- */
    arc_cct = create_gauge_arc(parent, 120, C_ORANGE, 2700, 6500);
    lv_obj_align(arc_cct, LV_ALIGN_TOP_RIGHT, -10, 10);

    /* 色温数值 */
    lbl_cct_v = lv_label_create(parent);
    lv_label_set_text(lbl_cct_v, "4600");
    style_label(lbl_cct_v, C_ORANGE, FONT_XLARGE);
    lv_obj_align(lbl_cct_v, LV_ALIGN_TOP_RIGHT, -10 - 60, 10 + 42);

    lbl_cct_u = lv_label_create(parent);
    lv_label_set_text(lbl_cct_u, "K");
    style_label(lbl_cct_u, C_DIM, FONT_NORMAL);
    lv_obj_align(lbl_cct_u, LV_ALIGN_TOP_RIGHT, -10 - 60, 10 + 68);

    /* ---- 底部数据卡片 ---- */
    lv_coord_t cy = 155;
    lv_coord_t cw = 68;
    lv_coord_t gap = 8;
    lv_coord_t x0 = 8;

    card_v  = make_data_card(parent, "V", x0, cy, cw);
    card_a  = make_data_card(parent, "A", x0 + cw + gap, cy, cw);
    card_w  = make_data_card(parent, "W", x0 + (cw + gap) * 2, cy, cw);

    cy += 52;
    card_lt = make_data_card(parent, "LED\xC2\xB0""C", x0, cy, cw);
    card_et = make_data_card(parent, "ENV\xC2\xB0""C", x0 + cw + gap, cy, cw);
}

void dashboard_update_brt_cct(uint8_t brt, uint16_t cct)
{
    if (!arc_brt || !arc_cct) return;

    lv_arc_set_value(arc_brt, brt);
    lv_arc_set_value(arc_cct, cct);

    char buf[8];
    snprintf(buf, sizeof(buf), "%d", brt);
    lv_label_set_text(lbl_brt_v, buf);

    snprintf(buf, sizeof(buf), "%u", cct);
    lv_label_set_text(lbl_cct_v, buf);

    /* 色温颜色联动 */
    lv_color_t col = cct_to_color(cct);
    lv_obj_set_style_arc_color(arc_cct, col, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(lbl_cct_v, col, 0);
}

void dashboard_update_power(float v, float a, float w)
{
    char buf[12];
    if (card_v)  { snprintf(buf, sizeof(buf), "%.1f", v); lv_label_set_text(card_v, buf); }
    if (card_a)  { snprintf(buf, sizeof(buf), "%.2f", a); lv_label_set_text(card_a, buf); }
    if (card_w)  { snprintf(buf, sizeof(buf), "%.1f", w); lv_label_set_text(card_w, buf); }
}

void dashboard_update_led_temp(float temp)
{
    char buf[12];
    if (card_lt) { snprintf(buf, sizeof(buf), "%.0f", temp); lv_label_set_text(card_lt, buf); }
}

void dashboard_update_env(float temp, float pressure)
{
    char buf[12];
    if (card_et) { snprintf(buf, sizeof(buf), "%.1f", temp); lv_label_set_text(card_et, buf); }
}
