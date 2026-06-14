/* ================================================================
 *  lvgl_ui.cpp - LVGL UI 入口 (多页面版本)
 *  集成页面管理器, 处理跨线程数据请求
 * ================================================================ */

#include "lvgl_ui.h"
#include "ui_page_mgr.h"

static volatile struct {
    bool has, has_s, has_t, has_e;
    uint8_t brt; uint16_t cct;
    float bv, bi, bw, btv, bti, btw, tmp, env_t, env_p;
} req = {};

void lvgl_ui_init(void)
{
    page_mgr_init();
}

void lvgl_ui_update(uint8_t brt, uint16_t cct)
{
    page_mgr_update_brt_cct(brt, cct);
}

void lvgl_ui_request(uint8_t brt, uint16_t cct)
{
    req.brt = brt;
    req.cct = cct;
    req.has = true;
}

void lvgl_ui_request_sensors(float bv, float bi, float bw,
                              float btv, float bti, float btw)
{
    req.bv = bv;
    req.bi = bi;
    req.bw = bw;
    req.btv = btv;
    req.bti = bti;
    req.btw = btw;
    req.has_s = true;
}

void lvgl_ui_request_temp(float t)
{
    req.tmp = t;
    req.has_t = true;
}

void lvgl_ui_request_env(float t, float p)
{
    req.env_t = t;
    req.env_p = p;
    req.has_e = true;
}

void lvgl_ui_process_updates(void)
{
    if (req.has) {
        req.has = false;
        page_mgr_update_brt_cct(req.brt, req.cct);
    }
    if (req.has_s) {
        req.has_s = false;
        page_mgr_update_power(req.bv, req.bi, req.bw);
    }
    if (req.has_t) {
        req.has_t = false;
        page_mgr_update_led_temp(req.tmp);
    }
    if (req.has_e) {
        req.has_e = false;
        page_mgr_update_env(req.env_t, req.env_p);
    }
}

void lvgl_ui_page_switch(int dir)
{
    page_mgr_switch(dir);
}
