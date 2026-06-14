#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void dashboard_create(lv_obj_t *parent);
void dashboard_update_brt_cct(uint8_t brt, uint16_t cct);
void dashboard_update_power(float v, float a, float w);
void dashboard_update_led_temp(float temp);
void dashboard_update_env(float temp, float pressure);

#ifdef __cplusplus
}
#endif
