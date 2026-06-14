/* ================================================================
 *  lvgl_indev.cpp - LVGL 输入设备 GPIO 初始化 (简化版)
 *  实际的编码器和按键轮询在 main.cpp 的 lvgl_task 主循环中进行
 * ================================================================ */

#include "lvgl_indev.h"
#include "config.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "LVGL_INDEV";

void lvgl_indev_init(void)
{
    ESP_LOGI(TAG, "Input device init");

    /* 编码器 GPIO (上拉输入) */
    gpio_config_t ec_cfg = {};
    ec_cfg.pin_bit_mask = (1ULL << EC1_A)  | (1ULL << EC1_B) |
                          (1ULL << EC2_A)  | (1ULL << EC2_B);
    ec_cfg.mode         = GPIO_MODE_INPUT;
    ec_cfg.pull_up_en   = GPIO_PULLUP_ENABLE;
    gpio_config(&ec_cfg);

    /* 按键 GPIO (上拉输入, 按下为低) */
    ec_cfg.pin_bit_mask = (1ULL << EC1_KEY) | (1ULL << EC2_KEY) | (1ULL << PIN_EN_KEY);
    gpio_config(&ec_cfg);

    ESP_LOGI(TAG, "Input device ready");
}
