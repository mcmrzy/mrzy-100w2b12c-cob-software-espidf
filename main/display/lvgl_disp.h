#pragma once

#include <stdint.h>
#include "lvgl.h"
#include "st7789v.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LVGL display buffer size in pixels (240 * 20 lines)
 */
#define LVGL_DISP_BUF_SIZE  (240 * 20)

/**
 * @brief Initialize LVGL display driver with ST7789V
 * @param display Pointer to initialized ST7789V instance
 */
void lvgl_disp_init(ST7789V *display);

#ifdef __cplusplus
}
#endif
