#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
#include "led_control.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the LVGL user interface
 *        Creates status bar, CCT arc card, brightness arc card, and temperature bar
 */
void lvgl_ui_init(void);

/**
 * @brief Update the CCT (Correlated Color Temperature) display
 * @param cct Color temperature value in Kelvin (2700-6500)
 */
void lvgl_ui_update_cct(uint16_t cct);

/**
 * @brief Update the brightness display
 * @param brt Brightness value (0-100)
 */
void lvgl_ui_update_brightness(uint8_t brt);

/**
 * @brief Update the temperature display
 * @param temp Temperature value in Celsius
 */
void lvgl_ui_update_temp(float temp);

/**
 * @brief Update the environment display (BMP280 data)
 * @param temp Environment temperature in Celsius
 * @param pressure Pressure in hPa
 */
void lvgl_ui_update_env(float temp, float pressure);

/**
 * @brief Update the sensor data cards
 * @param busV Bus voltage (V)
 * @param busI Bus current (A)
 * @param busW Bus power (W)
 * @param batV Battery voltage (V)
 * @param batI Battery current (A)
 * @param batW Battery power (W)
 */
void lvgl_ui_update_sensors(float busV, float busI, float busW,
                             float batV, float batI, float batW);

#ifdef __cplusplus
}
#endif
