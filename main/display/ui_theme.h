#pragma once

#include "lvgl.h"

/* ======================== 科技暗色主题色彩 ======================== */

#define C_BG            lv_color_hex(0x0A0A12)   /* 深黑背景 */
#define C_CARD          lv_color_hex(0x12121E)   /* 卡片背景 */
#define C_CARD_BORDER   lv_color_hex(0x1E1E30)   /* 卡片边框 */
#define C_CYAN          lv_color_hex(0x00E5FF)   /* 主强调色 (青色霓虹) */
#define C_ORANGE        lv_color_hex(0xFF8C00)   /* 暖色温色 */
#define C_BLUE          lv_color_hex(0x4499EE)   /* 冷色温色 */
#define C_GREEN         lv_color_hex(0x00E676)   /* 正常状态 */
#define C_RED           lv_color_hex(0xFF1744)   /* 警告状态 */
#define C_YELLOW        lv_color_hex(0xFFD600)   /* 注意状态 */
#define C_WHITE         lv_color_hex(0xEEEEEE)   /* 主文字 */
#define C_DIM           lv_color_hex(0x666688)   /* 次要文字 */
#define C_BAR_BG        lv_color_hex(0x1A1A28)   /* 进度条/弧背景 */
#define C_ARC_TRACK     lv_color_hex(0x1A1A2E)   /* 弧轨道色 */

/* ======================== 字体快捷宏 ======================== */

#define FONT_SMALL      (&lv_font_montserrat_12)
#define FONT_NORMAL     (&lv_font_montserrat_14)
#define FONT_MEDIUM     (&lv_font_montserrat_16)
#define FONT_LARGE      (&lv_font_montserrat_20)
#define FONT_XLARGE     (&lv_font_montserrat_24)

/* ======================== 页面尺寸 ======================== */

#define SCREEN_W        240
#define SCREEN_H        320
#define STATUS_BAR_H    28
#define PAGE_Y          STATUS_BAR_H
#define PAGE_H          (SCREEN_H - STATUS_BAR_H)

/* ======================== 通用样式工具 ======================== */

static inline void style_card(lv_obj_t *obj)
{
    lv_obj_set_style_bg_color(obj, C_CARD, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(obj, C_CARD_BORDER, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_radius(obj, 8, 0);
    lv_obj_set_style_pad_all(obj, 6, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

static inline void style_label(lv_obj_t *lbl, lv_color_t color, const lv_font_t *font)
{
    lv_obj_set_style_text_color(lbl, color, 0);
    lv_obj_set_style_text_font(lbl, font, 0);
}

/* 色温→颜色映射 (2700K琥珀 → 4500K暖白 → 6500K蓝白) */
static inline lv_color_t cct_to_color(uint16_t cct)
{
    static const lv_color_t cols[] = {
        lv_color_hex(0xFF9B3A),  /* 2700K */
        lv_color_hex(0xFFF5E6),  /* 4500K */
        lv_color_hex(0xD4E2FF),  /* 6500K */
    };
    if (cct <= 2700) return cols[0];
    if (cct >= 6500) return cols[2];
    if (cct <= 4500)
        return lv_color_mix(cols[0], cols[1], (uint8_t)((cct - 2700) * 255 / 1800));
    else
        return lv_color_mix(cols[1], cols[2], (uint8_t)((cct - 4500) * 255 / 2000));
}

/* 创建一个标准 lv_arc 仪表盘 (270°范围, 居中放置) */
static inline lv_obj_t* create_gauge_arc(lv_obj_t *parent, lv_coord_t size,
                                          lv_color_t indic_color, int min_val, int max_val)
{
    lv_obj_t *arc = lv_arc_create(parent);
    lv_obj_set_size(arc, size, size);
    lv_arc_set_bg_angles(arc, 135, 405);
    lv_arc_set_range(arc, min_val, max_val);
    lv_arc_set_value(arc, min_val);
    lv_arc_set_mode(arc, LV_ARC_MODE_NORMAL);

    /* 轨道样式 */
    lv_obj_set_style_arc_color(arc, C_ARC_TRACK, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 8, LV_PART_MAIN);

    /* 指示器样式 */
    lv_obj_set_style_arc_color(arc, indic_color, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, 8, LV_PART_INDICATOR);

    /* 旋钮隐藏 */
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(arc, 0, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);

    return arc;
}
