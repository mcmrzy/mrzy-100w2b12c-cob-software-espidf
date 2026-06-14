#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PAGE_COUNT  4

enum {
    PAGE_DASHBOARD = 0,
    PAGE_CONTROL   = 1,
    PAGE_SENSORS   = 2,
    PAGE_SETTINGS  = 3,
};

/* 页面管理器初始化 (创建状态栏 + 4个页面容器) */
void page_mgr_init(void);

/* 切换页面 dir: +1=下一页, -1=上一页, 0=跳到首页 */
void page_mgr_switch(int dir);

/* 更新当前页面的显示数据 */
void page_mgr_update_brt_cct(uint8_t brt, uint16_t cct);
void page_mgr_update_power(float v, float a, float w);
void page_mgr_update_led_temp(float temp);
void page_mgr_update_env(float temp, float pressure);

/* 获取当前页面索引 */
int page_mgr_current(void);

#ifdef __cplusplus
}
#endif
