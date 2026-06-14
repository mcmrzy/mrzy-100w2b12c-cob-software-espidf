/* ================================================================
 *  main.cpp - CobLux v2.0 | ESP32-S3 COB LED Controller
 *  商业化版本: 看门狗/温度保护/开机渐亮/长按/屏幕超时/模式切换/故障检测
 * ================================================================ */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_event.h"
#include "esp_task_wdt.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "config.h"
#include "storage.h"
#include "led_control.h"
#include "sensors.h"
#include "st7789v.h"
#include "lvgl_disp.h"
#include "lvgl_ui.h"

static const char *TAG = "MAIN";
static volatile bool system_ready = false;
static ST7789V display;

/* ======================== 开机/电源 ======================== */
static bool    first_wake = true;
static uint8_t saved_brt  = 50;

/* ======================== 节流存储 ======================== */
static uint32_t last_save_ms = 0;
static void save_throttled(void)
{
    uint32_t now = esp_timer_get_time() / 1000;
    if (now - last_save_ms > 2000) {
        storage_save(g_settings);
        last_save_ms = now;
    }
}

/* ======================== 编码器模式 ======================== */
typedef enum { ENC_MODE_BRT = 0, ENC_MODE_CCT, ENC_MODE_SCENE, ENC_MODE_COUNT } enc_mode_t;
static enc_mode_t enc_mode = ENC_MODE_BRT;

/* ======================== 屏幕超时 ======================== */
static uint32_t last_input_ms = 0;
static bool     screen_on     = true;
#define SCREEN_DIM_MS   10000   /* 10秒后变暗 */
#define SCREEN_OFF_MS   30000   /* 30秒后熄屏 */

/* ======================== 温度保护 ======================== */
static bool     thermal_protect = false;  /* true = 已触发过温降功率 */

/* ======================== LED 故障 ======================== */
static bool     led_fault = false;

/* ======================== 输入事件 ======================== */
typedef enum { EV_ENC1, EV_ENC2, EV_KEY, EV_KEY_LONG } ev_type_t;
typedef struct { ev_type_t t; int delta; } input_ev_t;
static QueueHandle_t input_queue = NULL;

/* ======================== 恢复/唤醒 ======================== */
static void wake_screen(void)
{
    last_input_ms = esp_timer_get_time() / 1000000;
    if (!screen_on) {
        screen_on = true;
        /* 屏幕唤醒, UI 会自动重绘 */
    }
}

static void revive(void)
{
    wake_screen();
    if (!first_wake) return;
    first_wake = false;
    g_settings.brightness = saved_brt;
    led_set_brightness(saved_brt);
    buzzer_beep(3000, 50);
}

/* ================================================================
 *  输入任务 (Core 0, 1kHz) - 编码器轮询 + 长按检测
 * ================================================================ */
typedef struct { int32_t cnt, last; uint8_t st; } enc_t;
static void enc_poll(enc_t *e, int a, int b)
{
    static const int8_t tbl[] = {0,1,-1,0,-1,0,0,1,1,0,0,-1,0,-1,1,0};
    e->st = (uint8_t)((e->st << 2) | ((a ? 1 : 0) << 1) | (b ? 1 : 0)) & 0x0F;
    e->cnt += tbl[e->st];
}

static void input_task(void *pv)
{
    /* 注册任务看门狗 */
    esp_task_wdt_add(NULL);

    /* GPIO 配置 */
    gpio_config_t cfg = {};
    cfg.mode = GPIO_MODE_INPUT;
    cfg.pull_up_en = GPIO_PULLUP_ENABLE;
    cfg.pin_bit_mask = (1ULL<<EC1_A)|(1ULL<<EC1_B)|(1ULL<<EC2_A)|(1ULL<<EC2_B);
    gpio_config(&cfg);

    gpio_pullup_en((gpio_num_t)EC1_A);
    gpio_pullup_en((gpio_num_t)EC1_B);
    gpio_pullup_en((gpio_num_t)EC2_A);
    gpio_pullup_en((gpio_num_t)EC2_B);

    cfg.pin_bit_mask = (1ULL<<EC1_KEY)|(1ULL<<EC2_KEY)|(1ULL<<PIN_EN_KEY);
    gpio_config(&cfg);
    gpio_pullup_en((gpio_num_t)EC2_KEY);

    enc_t e1 = {}, e2 = {};
    bool last_btn[3] = {false, false, false};
    uint8_t btn_db[3] = {0};
    uint16_t btn_hold[3] = {0};  /* 长按计数 (ms) */
    TickType_t lw = xTaskGetTickCount();

    vTaskDelay(pdMS_TO_TICKS(200));

    while (1) {
        vTaskDelayUntil(&lw, pdMS_TO_TICKS(1));

        /* 喂狗 */
        esp_task_wdt_reset();

        /* 编码器 */
        enc_poll(&e1, gpio_get_level((gpio_num_t)EC1_A),
                       gpio_get_level((gpio_num_t)EC1_B));
        enc_poll(&e2, gpio_get_level((gpio_num_t)EC2_A),
                       gpio_get_level((gpio_num_t)EC2_B));

        int d1 = e1.cnt - e1.last;
        int d2 = e2.cnt - e2.last;
        if      (d1 >= 4) { input_ev_t ev = {EV_ENC1, 1};  xQueueSend(input_queue,&ev,0); e1.last += 4; }
        else if (d1 <= -4){ input_ev_t ev = {EV_ENC1, -1}; xQueueSend(input_queue,&ev,0); e1.last -= 4; }
        if      (d2 >= 4) { input_ev_t ev = {EV_ENC2, 1};  xQueueSend(input_queue,&ev,0); e2.last += 4; }
        else if (d2 <= -4){ input_ev_t ev = {EV_ENC2, -1}; xQueueSend(input_queue,&ev,0); e2.last -= 4; }

        /* 按键: 短按 + 长按检测 */
        bool cur[3];
        cur[0] = (gpio_get_level((gpio_num_t)EC1_KEY) == 0);
        cur[1] = (gpio_get_level((gpio_num_t)EC2_KEY) == 0);
        cur[2] = (gpio_get_level((gpio_num_t)PIN_EN_KEY) == 0);

        for (int i = 0; i < 3; i++) {
            if (cur[i]) {
                if (++btn_db[i] >= 3) {
                    btn_db[i] = 3;
                    last_btn[i] = true;
                    btn_hold[i]++;
                    /* EN_KEY 长按 1秒 → 回首页 */
                    if (i == 2 && btn_hold[i] == 1000) {
                        input_ev_t ev = {EV_KEY_LONG, i};
                        xQueueSend(input_queue, &ev, 0);
                    }
                }
            } else {
                if (last_btn[i] && btn_hold[i] < 1000 && btn_hold[i] >= 3) {
                    /* 短按释放 */
                    input_ev_t ev = {EV_KEY, i};
                    xQueueSend(input_queue, &ev, 0);
                }
                btn_db[i] = 0;
                last_btn[i] = false;
                btn_hold[i] = 0;
            }
        }
    }
}

/* ================================================================
 *  输入处理 (lvgl_task 线程内)
 * ================================================================ */
static void on_encoder(int id, int delta)
{
    if (!system_ready) return;
    revive();

    if (id == 0) {
        /* EC1: 根据当前模式调节 */
        switch (enc_mode) {
        case ENC_MODE_BRT: {
            int b = (int)g_settings.brightness + delta;
            if (b > BRT_MAX) b = BRT_MAX;
            if (b < BRT_MIN) b = BRT_MIN;
            g_settings.brightness = (uint8_t)b;
            led_set_brightness(g_settings.brightness);
            buzzer_beep(4000, 12);
            break;
        }
        case ENC_MODE_CCT: {
            int c = (int)g_settings.colorTemp + delta * 100;
            if (c > CCT_MAX) c = CCT_MAX;
            if (c < CCT_MIN) c = CCT_MIN;
            g_settings.colorTemp = (uint16_t)c;
            led_set_cct(g_settings.colorTemp);
            buzzer_beep(4500, 12);
            break;
        }
        case ENC_MODE_SCENE: {
            static int scene_idx = 0;
            scene_idx += delta;
            if (scene_idx < 0) scene_idx = SCENE_COUNT - 1;
            if (scene_idx >= SCENE_COUNT) scene_idx = 0;
            g_settings.colorTemp = SCENES[scene_idx][0];
            g_settings.brightness = SCENES[scene_idx][1];
            led_set_cct(g_settings.colorTemp);
            led_set_brightness(g_settings.brightness);
            buzzer_beep(5000, 20);
            break;
        }
        default: break;
        }
    } else {
        /* EC2: 始终调色温 (保留快捷通道) */
        int c = (int)g_settings.colorTemp + delta * 100;
        if (c > CCT_MAX) c = CCT_MAX;
        if (c < CCT_MIN) c = CCT_MIN;
        g_settings.colorTemp = (uint16_t)c;
        led_set_cct(g_settings.colorTemp);
        buzzer_beep(4500, 12);
    }
    lvgl_ui_update(g_settings.brightness, g_settings.colorTemp);
    save_throttled();
}

static void on_key(int id)
{
    if (!system_ready) return;
    if (first_wake) {
        revive();
        lvgl_ui_update(g_settings.brightness, g_settings.colorTemp);
        return;
    }
    wake_screen();

    if (id == 0) {
        /* EC1_KEY: 切换编码器模式 */
        enc_mode = (enc_mode_t)((enc_mode + 1) % ENC_MODE_COUNT);
        buzzer_beep(3500, 15);
        /* UI 反馈: 短暂显示模式名 (通过更新 UI) */
    } else if (id == 1) {
        /* EC2_KEY: 切换页面 */
        lvgl_ui_page_switch(1);
        buzzer_beep(3500, 15);
    } else {
        /* EN_KEY 短按: 电源开关 */
        g_settings.brightness = (g_settings.brightness > 0) ? 0 : saved_brt;
        led_set_brightness(g_settings.brightness);
        buzzer_beep(5000, 40);
    }
    lvgl_ui_update(g_settings.brightness, g_settings.colorTemp);
    save_throttled();
}

static void on_key_long(int id)
{
    if (!system_ready) return;
    wake_screen();

    if (id == 2) {
        /* EN_KEY 长按: 回到首页 */
        lvgl_ui_page_switch(0);
        buzzer_beep(3000, 30);
    }
}

/* ================================================================
 *  LVGL Task (Core 1) - 带看门狗 + 屏幕超时
 * ================================================================ */
static void lvgl_task(void *pv)
{
    display.begin();
    display.fillScreen(COLOR_BLACK);

    lv_init();
    lvgl_disp_init(&display);
    lvgl_ui_init();

    /* 注册任务看门狗 */
    esp_task_wdt_add(NULL);

    uint32_t last_tick = esp_timer_get_time() / 1000;
    last_input_ms = esp_timer_get_time() / 1000000;

    while (1) {
        uint32_t now = esp_timer_get_time() / 1000;
        lv_tick_inc(now - last_tick);
        last_tick = now;

        /* 处理输入事件 */
        input_ev_t ev;
        while (xQueueReceive(input_queue, &ev, 0)) {
            switch (ev.t) {
            case EV_KEY:      on_key(ev.delta); break;
            case EV_KEY_LONG: on_key_long(ev.delta); break;
            case EV_ENC1:     on_encoder(0, ev.delta); break;
            case EV_ENC2:     on_encoder(1, ev.delta); break;
            }
        }

        /* 屏幕超时检测 */
        uint32_t now_s = esp_timer_get_time() / 1000000;
        uint32_t idle_ms = (now_s - last_input_ms) * 1000;
        if (screen_on && idle_ms > SCREEN_OFF_MS) {
            screen_on = false;
            /* 熄屏: 清屏为黑色 */
            display.fillScreen(COLOR_BLACK);
        }

        /* 更新 UI 数据 */
        if (screen_on) {
            lvgl_ui_process_updates();
        }

        /* 渲染 */
        uint32_t ms = lv_timer_handler();
        if (ms < 5) ms = 5; else if (ms > 20) ms = 20;

        vTaskDelay(pdMS_TO_TICKS(ms));

        /* 喂狗 */
        esp_task_wdt_reset();
    }
}

/* ================================================================
 *  Control Task (Core 0) - 看门狗 + 温度保护 + 开机渐亮 + LED故障检测
 * ================================================================ */
static void ctrl_task(void *pv)
{
    vTaskDelay(pdMS_TO_TICKS(500));

    /* 注册任务看门狗 */
    esp_task_wdt_add(NULL);

    led_init();
    led_set_cct(g_settings.colorTemp);

    /* 保存用户亮度, 初始为0 (安全) */
    saved_brt = g_settings.brightness;
    g_settings.brightness = 0;
    led_set_brightness(0);

    fan_init();
    buzzer_init();
    sensors_init();

    /* ---- 开机渐亮动画 (1.5秒) ---- */
    if (saved_brt > 0) {
        ESP_LOGI(TAG, "Boot fade-in: 0 -> %d%% over 1.5s", saved_brt);
        uint8_t step = 0;
        uint8_t steps = 30;  /* 30步 * 50ms = 1.5s */
        for (step = 1; step <= steps; step++) {
            uint8_t val = (uint8_t)((uint32_t)saved_brt * step / steps);
            led_set_brightness(val);
            g_settings.brightness = val;
            lvgl_ui_request(val, g_settings.colorTemp);
            vTaskDelay(pdMS_TO_TICKS(50));
            esp_task_wdt_reset();
        }
    }

    lvgl_ui_request(g_settings.brightness, g_settings.colorTemp);
    system_ready = true;
    ESP_LOGI(TAG, "System ready");

    uint32_t fault_count = 0;

    TickType_t lw = xTaskGetTickCount();
    while (1) {
        vTaskDelayUntil(&lw, pdMS_TO_TICKS(100));
        esp_task_wdt_reset();

        /* ---- 传感器读取 ---- */
        float vb, ib, pb, vbt, ibt, pbt;
        if (sensors_read_power(&vb, &ib, &pb, &vbt, &ibt, &pbt) == ESP_OK) {
            lvgl_ui_request_sensors(vb, ib, pb, vbt, ibt, pbt);

            /* ---- LED 故障检测 ---- */
            if (g_settings.brightness > 5 && !thermal_protect) {
                /* 根据亮度计算预期电流下限 (粗略估计: 最低20% 满载电流) */
                float expected_min = (float)(g_settings.brightness) * 0.02f;
                if (ib < expected_min) {
                    /* 亮度>5% 但电流远低于预期 → LED 开路 */
                    fault_count++;
                    if (fault_count > 20) {  /* 持续2秒确认 */
                        led_fault = true;
                        ESP_LOGE(TAG, "LED FAULT: open circuit! BRT=%d, I=%.3fA (min=%.3fA)",
                                 g_settings.brightness, ib, expected_min);
                    }
                } else if (ib > 4.0f) {
                    /* 电流超限 → 过流保护 */
                    led_fault = true;
                    g_settings.brightness = 0;
                    led_set_brightness(0);
                    ESP_LOGE(TAG, "LED FAULT: overcurrent! I=%.2fA", ib);
                } else {
                    fault_count = 0;
                    led_fault = false;
                }
            }
        }

        /* ---- 温度保护 ---- */
        float tl, te, pr;
        if (sensors_read_env(&tl, &te, &pr) == ESP_OK) {
            lvgl_ui_request_temp(tl);
            lvgl_ui_request_env(te, pr);
            fan_update(tl);

            /* 温度回退 (忽略无效读数: NTC未接时返回 999/-999) */
            bool temp_valid = (tl > -50.0f && tl < 200.0f);
            if (temp_valid && tl >= TEMP_SHUTDOWN) {
                /* 过温关断 */
                if (!thermal_protect) {
                    ESP_LOGE(TAG, "THERMAL SHUTDOWN: %.0f°C >= %d°C", tl, TEMP_SHUTDOWN);
                    thermal_protect = true;
                }
                g_settings.brightness = 0;
                led_set_brightness(0);
            } else if (temp_valid && tl >= TEMP_WARN) {
                /* 高温降功率: 线性降到 50% */
                if (!thermal_protect) {
                    ESP_LOGW(TAG, "Thermal derating at %.0f°C", tl);
                    thermal_protect = true;
                }
                float ratio = 1.0f - (tl - TEMP_WARN) / (float)(TEMP_SHUTDOWN - TEMP_WARN) * 0.5f;
                uint8_t max_brt = (uint8_t)(saved_brt * ratio);
                if (max_brt < 5) max_brt = 5;
                if (g_settings.brightness > max_brt) {
                    g_settings.brightness = max_brt;
                    led_set_brightness(max_brt);
                }
            } else if (!temp_valid || tl < TEMP_WARN - 5) {
                /* 温度恢复正常, 允许恢复亮度 */
                if (thermal_protect) {
                    ESP_LOGI(TAG, "Thermal recovery at %.0f°C", tl);
                    thermal_protect = false;
                }
            }
        }
    }
}

/* ================================================================
 *  Main
 * ================================================================ */
extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "===== CobLux v2.0 =====");
    nvs_flash_init();
    esp_event_loop_create_default();
    storage_init();

    input_queue = xQueueCreate(64, sizeof(input_ev_t));

    xTaskCreatePinnedToCore(lvgl_task,  "lvgl",  8192, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(ctrl_task,  "ctrl",  4096, NULL, 4, NULL, 0);
    xTaskCreatePinnedToCore(input_task, "input", 4096, NULL, 7, NULL, 0);
}
