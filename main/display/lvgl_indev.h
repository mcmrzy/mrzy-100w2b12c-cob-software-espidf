#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化输入 GPIO (编码器A/B + 按键)
 */
void lvgl_indev_init(void);

#ifdef __cplusplus
}
#endif
