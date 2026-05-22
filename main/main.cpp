/**
 * @file main.cpp
 * @brief COB-LED 控制器主入口
 *
 * ESP-IDF v5.4 + ESP32-S3
 * FreeRTOS 多任务架构:
 *   - lvgl_task    (Core 1): LVGL UI 刷新 + 编码器输入
 *   - control_task (Core 0): 传感器读取 + 温度保护 + 自动亮度 + 风扇 + MQTT
 *   - wifi_task    (Core 0): WiFi + Web UI + BLE + MQTT 初始化
 */

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

#include "config.h"
#include "storage.h"
#include "led_control.h"
#include "sensors.h"
#include "st7789v.h"
#include "lvgl_disp.h"
#include "lvgl_indev.h"
#include "lvgl_ui.h"
#include "web_ui.h"
#include "ble_ui.h"
#include "app_mqtt.h"

static const char *TAG = "MAIN";

/* ======================== 全局对象 ======================== */

static ST7789V tft;  /* ST7789V 显示屏实例 */

/* ======================== 编码器读取 ======================== */

/**
 * @brief 读取编码器状态并喂给 LVGL
 *
 * 使用 GPIO 直接读取编码器 A/B 相和按键。
 * 简单的状态机实现旋转方向检测。
 */
static void read_encoder1(void)
{
    static uint8_t last_ab = 0;
    static int8_t enc_states[] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

    uint8_t a = gpio_get_level((gpio_num_t)EC1_A);
    uint8_t b = gpio_get_level((gpio_num_t)EC1_B);
    uint8_t ab = (a << 1) | b;

    if (ab != last_ab) {
        int8_t delta = enc_states[last_ab * 4 + ab];
        if (delta != 0) {
            lvgl_indev_feed_encoder1(delta);
        }
        last_ab = ab;
    }

    /* 按键 */
    lvgl_indev_feed_key1(gpio_get_level((gpio_num_t)EC1_KEY) == 0);
}

static void read_encoder2(void)
{
    static uint8_t last_ab = 0;
    static int8_t enc_states[] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

    uint8_t a = gpio_get_level((gpio_num_t)EC2_A);
    uint8_t b = gpio_get_level((gpio_num_t)EC2_B);
    uint8_t ab = (a << 1) | b;

    if (ab != last_ab) {
        int8_t delta = enc_states[last_ab * 4 + ab];
        if (delta != 0) {
            lvgl_indev_feed_encoder2(delta);
        }
        last_ab = ab;
    }

    /* 按键 */
    lvgl_indev_feed_key2(gpio_get_level((gpio_num_t)EC2_KEY) == 0);
}

/* ======================== 蜂鸣器 ======================== */

static void buzzer_beep(void)
{
    /* 使用 LEDC Timer1 / Channel1 输出蜂鸣器 PWM */
    ledc_timer_config_t timer_cfg = {};
    timer_cfg.speed_mode      = LEDC_LOW_SPEED_MODE;
    timer_cfg.duty_resolution = LEDC_TIMER_10_BIT;
    timer_cfg.timer_num       = LEDC_TIMER_1;
    timer_cfg.freq_hz         = g_settings.buzzerFreq;
    timer_cfg.clk_cfg         = LEDC_AUTO_CLK;
    timer_cfg.deconfigure     = false;
    ledc_timer_config(&timer_cfg);

    ledc_channel_config_t ch_cfg = {};
    ch_cfg.gpio_num   = PIN_BUZZER;
    ch_cfg.speed_mode = LEDC_LOW_SPEED_MODE;
    ch_cfg.channel    = LEDC_CHANNEL_1;
    ch_cfg.intr_type  = LEDC_INTR_DISABLE;
    ch_cfg.timer_sel  = LEDC_TIMER_1;
    ch_cfg.duty       = g_settings.buzzerDuty;
    ch_cfg.hpoint     = 0;
    ch_cfg.sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD;
    ch_cfg.flags.output_invert = 0;
    ledc_channel_config(&ch_cfg);

    /* 鸣叫 BUZZER_DURATION 毫秒后关闭 */
    vTaskDelay(pdMS_TO_TICKS(BUZZER_DURATION));

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
}

/* ======================== GPIO 初始化 ======================== */

static void gpio_init(void)
{
    /* 编码器引脚: 输入 + 上拉 */
    gpio_config_t enc_cfg = {
        .pin_bit_mask = (1ULL << EC1_A) | (1ULL << EC1_B) | (1ULL << EC1_KEY) |
                         (1ULL << EC2_A) | (1ULL << EC2_B) | (1ULL << EC2_KEY) |
                         (1ULL << PIN_EN_KEY),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&enc_cfg);

    /* 蜂鸣器引脚: 输出 */
    gpio_config_t buz_cfg = {
        .pin_bit_mask = (1ULL << PIN_BUZZER),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&buz_cfg);
    gpio_set_level((gpio_num_t)PIN_BUZZER, 0);
}

/* ======================== LVGL 任务 (Core 1) ======================== */

static void lvgl_task(void *arg)
{
    ESP_LOGI(TAG, "LVGL task started on core %d", xPortGetCoreID());

    /* 初始化显示屏 */
    if (!tft.begin()) {
        ESP_LOGE(TAG, "ST7789V init failed!");
        vTaskDelete(NULL);
        return;
    }

    /* 设置横屏 */
    tft.setRotation(1);

    /* 初始化 LVGL 显示驱动 */
    lvgl_disp_init(&tft);

    /* 初始化 LVGL 输入设备 */
    lvgl_indev_init();

    /* 创建 UI */
    lvgl_ui_init();

    /* 用存储的设置更新 UI */
    lvgl_ui_update_cct(g_colorTemp);
    lvgl_ui_update_brightness(g_brightness);

    /* LVGL 主循环 */
    uint32_t last_ui_update = 0;
    while (1) {
        /* 读取编码器 */
        read_encoder1();
        read_encoder2();

        /* LVGL 定时处理 (5ms) */
        lv_timer_handler();

        /* 每 500ms 更新 UI 显示 */
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (now - last_ui_update >= 500) {
            last_ui_update = now;
            lvgl_ui_update_temp(g_sensors.temperature);
            /* 更新 BMP280 环境数据 */
            lvgl_ui_update_env(g_sensors.env_temperature, g_sensors.pressure);
            /* 更新传感器数据卡片 */
            lvgl_ui_update_sensors(
                g_sensors.bus_voltage, g_sensors.current_in, g_sensors.power_in,
                g_sensors.bat_voltage, g_sensors.current_bat, g_sensors.power_bat);
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

/* ======================== 控制任务 (Core 0) ======================== */

static void control_task(void *arg)
{
    ESP_LOGI(TAG, "Control task started on core %d", xPortGetCoreID());

    uint32_t last_sensor_read = 0;
    uint32_t last_auto_bright = 0;
    uint32_t last_mqtt_publish = 0;
    bool temp_shutdown = false;

    while (1) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

        /* ---- 每 1000ms 读取传感器 ---- */
        if (now - last_sensor_read >= 1000) {
            last_sensor_read = now;
            sensors_read();

            float temp = g_sensors.temperature;

            /* 温度保护 */
            if (temp >= TEMP_SHUTDOWN) {
                if (!temp_shutdown) {
                    ESP_LOGE(TAG, "TEMP SHUTDOWN: %.1f C, turning off LED!", temp);
                    led_set_brightness(0);
                    temp_shutdown = true;
                }
            } else if (temp >= TEMP_WARN) {
                ESP_LOGW(TAG, "TEMP WARNING: %.1f C", temp);
                temp_shutdown = false;
            } else {
                temp_shutdown = false;
            }

            /* 风扇控制 */
            fan_update(temp);
        }

        /* ---- 自动亮度调节 ---- */
        if (g_auto_brightness && !temp_shutdown &&
            now - last_auto_bright >= AUTO_BRIGHT_INTERVAL) {
            last_auto_bright = now;

            /* 简单映射: LDR 值越高 (越亮) -> 亮度越低 */
            uint8_t new_brt;
            if (g_sensors.ldr_lux <= LDR_DARK_THRESHOLD) {
                new_brt = 100;  /* 暗环境: 全亮 */
            } else if (g_sensors.ldr_lux >= LDR_BRIGHT_THRESHOLD) {
                new_brt = 10;   /* 亮环境: 低亮 */
            } else {
                /* 线性映射 */
                new_brt = (uint8_t)(100 - (uint32_t)(g_sensors.ldr_lux - LDR_DARK_THRESHOLD) * 90 /
                                    (LDR_BRIGHT_THRESHOLD - LDR_DARK_THRESHOLD));
            }
            led_set_brightness(new_brt);
        }

        /* ---- MQTT 定期发布 (每 5s) ---- */
        if (g_mqtt_enabled && now - last_mqtt_publish >= 5000) {
            last_mqtt_publish = now;
            mqtt_loop();
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* ======================== WiFi/网络任务 (Core 0) ======================== */

static void wifi_task(void *arg)
{
    ESP_LOGI(TAG, "WiFi task started on core %d", xPortGetCoreID());

    /* 初始化 WiFi (AP+STA) */
    wifi_init();

    /* 等待 WiFi 就绪 */
    vTaskDelay(pdMS_TO_TICKS(1000));

    /* 初始化 Web UI (HTTP 服务器 + mDNS) */
    webui_init();

    /* 初始化 BLE */
    ble_init();

    /* 初始化 MQTT */
    mqtt_init();

    /* WiFi 任务完成, 删除自身 */
    ESP_LOGI(TAG, "Network init done, wifi task exiting");
    vTaskDelete(NULL);
}

/* ======================== app_main ======================== */

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  COB-LED Controller v1.0");
    ESP_LOGI(TAG, "  ESP32-S3 + ESP-IDF v5.4");
    ESP_LOGI(TAG, "========================================");

    /* 1. 初始化 NVS + 加载设置 */
    storage_init();
    ESP_LOGI(TAG, "Settings: CCT=%u, BRT=%u, AutoBrt=%d, MqttEn=%d",
             g_settings.colorTemp, g_settings.brightness,
             (int)g_settings.autoBright, (int)g_settings.mqttEnable);

    /* 2. 应用存储的设置到全局变量 */
    g_colorTemp  = g_settings.colorTemp;
    g_brightness = g_settings.brightness;
    g_auto_brightness = g_settings.autoBright;

    /* 3. 初始化 GPIO (编码器 + 蜂鸣器) */
    gpio_init();

    /* 4. 初始化 LED 驱动 */
    led_init();
    led_set_cct(g_colorTemp);
    led_set_brightness(g_brightness);

    /* 5. 初始化风扇 */
    fan_init();

    /* 6. 初始化传感器 */
    sensors_init();

    /* 7. 开机蜂鸣 */
    buzzer_beep();
    ESP_LOGI(TAG, "Startup beep done");

    /* 8. 创建 LVGL 任务 (Core 1, 高优先级) */
    xTaskCreatePinnedToCore(lvgl_task, "lvgl", 8192, NULL, 10, NULL, 1);

    /* 9. 创建控制任务 (Core 0) */
    xTaskCreatePinnedToCore(control_task, "control", 4096, NULL, 5, NULL, 0);

    /* 10. 创建 WiFi/网络任务 (Core 0) */
    xTaskCreatePinnedToCore(wifi_task, "wifi", 8192, NULL, 4, NULL, 0);

    ESP_LOGI(TAG, "All tasks created, app_main done");
}
