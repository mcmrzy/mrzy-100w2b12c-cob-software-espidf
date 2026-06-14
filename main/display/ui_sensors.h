#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void sensors_create(lv_obj_t *parent);
void sensors_update_power(float v, float a, float w);
void sensors_update_led_temp(float temp);
void sensors_update_env(float temp, float pressure);

#ifdef __cplusplus
}
#endif
