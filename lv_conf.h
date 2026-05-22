#pragma once
#ifndef LV_CONF_H
#define LV_CONF_H

// ==================== LVGL 配置 ====================

// 显示配置
#define LV_HOR_RES_MAX          240
#define LV_VER_RES_MAX          320
#define LV_COLOR_DEPTH          16
#define LV_COLOR_16_SWAP        1   // ESP32小端序，需要交换高低字节

// 内存配置
#define LV_MEM_CUSTOM           0
#define LV_MEM_SIZE             (64 * 1024U)  // 64KB

// 刷新周期
#define LV_DISP_DEF_REFR_PERIOD 16

// 输入设备
#define LV_USE_INDEV            1

// 主题
#define LV_USE_THEME_DEFAULT    1
#define LV_THEME_DEFAULT_INIT   lv_theme_default_init

// 字体
#define LV_FONT_MONTSERRAT_12   1
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_16   1
#define LV_FONT_MONTSERRAT_18   1
#define LV_FONT_MONTSERRAT_20   1
#define LV_FONT_MONTSERRAT_24   1

// 控件
#define LV_USE_BTN              1
#define LV_USE_LABEL            1
#define LV_USE_SLIDER           1
#define LV_USE_SWITCH           1
#define LV_USE_CHART            1
#define LV_USE_BAR              1
#define LV_USE_ARC              1
#define LV_USE_IMG              1
#define LV_USE_LIST             1
#define LV_USE_DROPDOWN         1
#define LV_USE_ROLLER           1
#define LV_USE_CHECKBOX         1

// 动画
#define LV_USE_ANIMATION        1

// GPU (ESP32-S3有PSRAM，可以加速)
#define LV_USE_GPU              0

// 日志
#define LV_USE_LOG              0

#endif // LV_CONF_H
