/* ================================================================
 *  ui_sensors.cpp - 传感器详情页 (页面2)
 *  4个圆弧仪表盘 (V/A/W/°C) + 底部环境数据
 * ================================================================ */

#include "ui_sensors.h"
#include "ui_theme.h"
#include <stdio.h>

static lv_obj_t *arc_v  = nullptr;
static lv_obj_t *arc_a  = nullptr;
static lv_obj_t *arc_w  = nullptr;
static lv_obj_t *arc_lt = nullptr;

static lv_obj_t *lbl_v  = nullptr;
static lv_obj_t *lbl_a  = nullptr;
static lv_obj_t *lbl_w  = nullptr;
static lv_obj_t *lbl_lt = nullptr;

static lv_obj_t *lbl_env_t = nullptr;
static lv_obj_t *lbl_env_p = nullptr;

static lv_obj_t* create_sensor_arc(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                    lv_coord_t size, lv_color_t color,
                                    const char *unit, int max_val)
{
    lv_obj_t *arc = create_gauge_arc(parent, size, color, 0, max_val);
    lv_obj_set_pos(arc, x, y);

    /* 单位标签 (弧中心) */
    lv_obj_t *u = lv_label_create(parent);
    lv_label_set_text(u, unit);
    style_label(u, C_DIM, FONT_SMALL);
    lv_obj_align(u, LV_ALIGN_TOP_LEFT, x + size / 2, y + size / 2 + 8);

    return arc;
}

void sensors_create(lv_obj_t *parent)
{
    lv_coord_t arc_sz = 95;
    lv_coord_t gap = 10;
    lv_coord_t x0 = 8;
    lv_coord_t y0 = 8;
    lv_coord_t x1 = x0 + arc_sz + gap;
    lv_coord_t y1 = y0 + arc_sz + gap;

    /* 4个仪表盘: 2x2 网格 */
    arc_v  = create_sensor_arc(parent, x0, y0, arc_sz, C_CYAN,  "V",   20);
    arc_a  = create_sensor_arc(parent, x1, y0, arc_sz, C_GREEN, "A",    5);
    arc_w  = create_sensor_arc(parent, x0, y1, arc_sz, C_YELLOW,"W",   50);
    arc_lt = create_sensor_arc(parent, x1, y1, arc_sz, C_RED,   "\xC2\xB0""C", 100);

    /* 数值标签 (弧中心, 在单位标签之上) */
    lbl_v  = lv_label_create(parent);
    style_label(lbl_v, C_WHITE, FONT_LARGE);
    lv_obj_align(lbl_v, LV_ALIGN_TOP_LEFT, x0 + arc_sz / 2, y0 + arc_sz / 2 - 12);

    lbl_a  = lv_label_create(parent);
    style_label(lbl_a, C_WHITE, FONT_LARGE);
    lv_obj_align(lbl_a, LV_ALIGN_TOP_LEFT, x1 + arc_sz / 2, y0 + arc_sz / 2 - 12);

    lbl_w  = lv_label_create(parent);
    style_label(lbl_w, C_WHITE, FONT_LARGE);
    lv_obj_align(lbl_w, LV_ALIGN_TOP_LEFT, x0 + arc_sz / 2, y1 + arc_sz / 2 - 12);

    lbl_lt = lv_label_create(parent);
    style_label(lbl_lt, C_WHITE, FONT_LARGE);
    lv_obj_align(lbl_lt, LV_ALIGN_TOP_LEFT, x1 + arc_sz / 2, y1 + arc_sz / 2 - 12);

    /* ---- 底部环境数据 ---- */
    lv_coord_t ey = y1 + arc_sz + 12;

    lv_obj_t *t_env = lv_label_create(parent);
    lv_label_set_text(t_env, "ENV");
    style_label(t_env, C_DIM, FONT_SMALL);
    lv_obj_set_pos(t_env, 16, ey);

    lbl_env_t = lv_label_create(parent);
    lv_label_set_text(lbl_env_t, "--.-\xC2\xB0""C");
    style_label(lbl_env_t, C_WHITE, FONT_MEDIUM);
    lv_obj_set_pos(lbl_env_t, 16, ey + 16);

    lbl_env_p = lv_label_create(parent);
    lv_label_set_text(lbl_env_p, "---- hPa");
    style_label(lbl_env_p, C_DIM, FONT_NORMAL);
    lv_obj_set_pos(lbl_env_p, 120, ey + 16);
}

static void set_arc_and_label(lv_obj_t *arc, lv_obj_t *lbl, float val,
                               const char *fmt, int arc_max)
{
    char buf[12];
    snprintf(buf, sizeof(buf), fmt, val);
    lv_label_set_text(lbl, buf);

    int v = (int)(val * 100 / arc_max);
    if (v > arc_max) v = arc_max;
    if (arc) lv_arc_set_value(arc, (int)val);
}

void sensors_update_power(float v, float a, float w)
{
    set_arc_and_label(arc_v, lbl_v, v, "%.1f", 20);
    set_arc_and_label(arc_a, lbl_a, a, "%.2f", 5);
    set_arc_and_label(arc_w, lbl_w, w, "%.1f", 50);
}

void sensors_update_led_temp(float temp)
{
    set_arc_and_label(arc_lt, lbl_lt, temp, "%.0f", 100);

    /* 温度警告变色 */
    if (temp >= 85)
        lv_obj_set_style_arc_color(arc_lt, C_RED, LV_PART_INDICATOR);
    else if (temp >= 70)
        lv_obj_set_style_arc_color(arc_lt, C_YELLOW, LV_PART_INDICATOR);
    else
        lv_obj_set_style_arc_color(arc_lt, C_GREEN, LV_PART_INDICATOR);
}

void sensors_update_env(float temp, float pressure)
{
    char buf[20];
    if (lbl_env_t) { snprintf(buf, sizeof(buf), "%.1f\xC2\xB0""C", temp); lv_label_set_text(lbl_env_t, buf); }
    if (lbl_env_p) { snprintf(buf, sizeof(buf), "%.1f hPa", pressure); lv_label_set_text(lbl_env_p, buf); }
}
