#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void lvgl_ui_init(void);

/* 直接更新 (lvgl_task 线程内调用) */
void lvgl_ui_update(uint8_t brt, uint16_t cct);

/* 跨线程请求 */
void lvgl_ui_request(uint8_t brt, uint16_t cct);
void lvgl_ui_request_sensors(float bv, float bi, float bw,
                              float btv, float bti, float btw);
void lvgl_ui_request_temp(float temp);
void lvgl_ui_request_env(float temp, float pressure);
void lvgl_ui_process_updates(void);

/* 页面切换 */
void lvgl_ui_page_switch(int dir);

#ifdef __cplusplus
}
#endif
