#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "config.h"

extern uint16_t g_colorTemp;
extern uint8_t  g_brightness;

void led_init(void);
void led_update(void);
void led_set_cct(uint16_t cct);
void led_set_brightness(uint8_t brt);

void fan_init(void);
void fan_update(float temp_c);

#ifdef __cplusplus
}
#endif
