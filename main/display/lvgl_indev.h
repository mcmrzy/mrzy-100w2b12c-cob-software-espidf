#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize LVGL input devices (2 encoders)
 */
void lvgl_indev_init(void);

/**
 * @brief Feed encoder 1 delta value (call from encoder ISR or task)
 * @param delta Encoder steps since last read (-N to +N)
 */
void lvgl_indev_feed_encoder1(int delta);

/**
 * @brief Feed encoder 2 delta value (call from encoder ISR or task)
 * @param delta Encoder steps since last read (-N to +N)
 */
void lvgl_indev_feed_encoder2(int delta);

/**
 * @brief Feed key 1 press/release state (call from button ISR or task)
 * @param pressed true if pressed, false if released
 */
void lvgl_indev_feed_key1(bool pressed);

/**
 * @brief Feed key 2 press/release state (call from button ISR or task)
 * @param pressed true if pressed, false if released
 */
void lvgl_indev_feed_key2(bool pressed);

#ifdef __cplusplus
}
#endif
