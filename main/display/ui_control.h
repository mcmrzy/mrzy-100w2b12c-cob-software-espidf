#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void control_create(lv_obj_t *parent);
void control_update_brt_cct(uint8_t brt, uint16_t cct);

#ifdef __cplusplus
}
#endif
