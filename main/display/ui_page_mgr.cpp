/* ================================================================
 *  ui_page_mgr.cpp - 页面管理器
 *  管理4个页面容器 + 状态栏 + 页面切换动画
 * ================================================================ */

#include "ui_page_mgr.h"
#include "ui_theme.h"
#include "ui_dashboard.h"
#include "ui_control.h"
#include "ui_sensors.h"
#include "ui_settings.h"
#include <stdio.h>

static lv_obj_t *status_bar = nullptr;
static lv_obj_t *lbl_title  = nullptr;
static lv_obj_t *lbl_temp   = nullptr;
static lv_obj_t *pages[PAGE_COUNT] = {};
static int current_page = PAGE_DASHBOARD;

static const char *page_titles[PAGE_COUNT] = {
    "CobLux", "Control", "Sensors", "Settings"
};

/* 页面创建回调 */
typedef void (*page_create_fn)(lv_obj_t *parent);
static const page_create_fn page_creators[PAGE_COUNT] = {
    dashboard_create,
    control_create,
    sensors_create,
    settings_create,
};

static void create_status_bar(lv_obj_t *parent)
{
    status_bar = lv_obj_create(parent);
    lv_obj_set_size(status_bar, SCREEN_W, STATUS_BAR_H);
    lv_obj_set_pos(status_bar, 0, 0);
    lv_obj_set_style_bg_color(status_bar, lv_color_hex(0x0E0E1A), 0);
    lv_obj_set_style_bg_opa(status_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(status_bar, 0, 0);
    lv_obj_set_style_radius(status_bar, 0, 0);
    lv_obj_set_style_pad_hor(status_bar, 12, 0);
    lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_SCROLLABLE);

    /* 标题 */
    lbl_title = lv_label_create(status_bar);
    lv_label_set_text(lbl_title, "CobLux");
    style_label(lbl_title, C_CYAN, FONT_MEDIUM);
    lv_obj_align(lbl_title, LV_ALIGN_LEFT_MID, 0, 0);

    /* LED 温度 */
    lbl_temp = lv_label_create(status_bar);
    lv_label_set_text(lbl_temp, "--.-\xC2\xB0""C");
    style_label(lbl_temp, C_DIM, FONT_NORMAL);
    lv_obj_align(lbl_temp, LV_ALIGN_RIGHT_MID, 0, 0);
}

void page_mgr_init(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, C_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    create_status_bar(scr);

    /* 创建4个页面容器 */
    for (int i = 0; i < PAGE_COUNT; i++) {
        pages[i] = lv_obj_create(scr);
        lv_obj_set_size(pages[i], SCREEN_W, PAGE_H);
        lv_obj_set_pos(pages[i], 0, PAGE_Y);
        lv_obj_set_style_bg_color(pages[i], C_BG, 0);
        lv_obj_set_style_bg_opa(pages[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(pages[i], 0, 0);
        lv_obj_set_style_radius(pages[i], 0, 0);
        lv_obj_set_style_pad_all(pages[i], 0, 0);
        lv_obj_clear_flag(pages[i], LV_OBJ_FLAG_SCROLLABLE);

        /* 创建页面内容 */
        page_creators[i](pages[i]);

        /* 隐藏非当前页 */
        if (i != current_page)
            lv_obj_add_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
    }
}

void page_mgr_switch(int dir)
{
    int new_page = current_page;
    if (dir == 0) {
        new_page = PAGE_DASHBOARD;
    } else {
        new_page = current_page + dir;
        if (new_page < 0) new_page = PAGE_COUNT - 1;
        if (new_page >= PAGE_COUNT) new_page = 0;
    }
    if (new_page == current_page) return;

    /* 隐藏旧页, 显示新页 */
    lv_obj_add_flag(pages[current_page], LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(pages[new_page], LV_OBJ_FLAG_HIDDEN);

    /* 更新状态栏标题 */
    lv_label_set_text(lbl_title, page_titles[new_page]);

    current_page = new_page;
}

int page_mgr_current(void)
{
    return current_page;
}

/* ---- 数据更新 (分发到各页面) ---- */

void page_mgr_update_brt_cct(uint8_t brt, uint16_t cct)
{
    dashboard_update_brt_cct(brt, cct);
    control_update_brt_cct(brt, cct);
}

void page_mgr_update_power(float v, float a, float w)
{
    dashboard_update_power(v, a, w);
    sensors_update_power(v, a, w);
}

void page_mgr_update_led_temp(float temp)
{
    dashboard_update_led_temp(temp);
    sensors_update_led_temp(temp);
    /* 更新状态栏温度 */
    if (lbl_temp) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f\xC2\xB0""C", temp);
        lv_label_set_text(lbl_temp, buf);
    }
}

void page_mgr_update_env(float temp, float pressure)
{
    dashboard_update_env(temp, pressure);
    sensors_update_env(temp, pressure);
}
