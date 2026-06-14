#include "sleep.h"
#include "config.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SLEEP";

void deep_sleep_enter(uint32_t seconds, int wakeup_pin)
{
    ESP_LOGI(TAG, "Entering deep sleep. Wakeup: %s",
             seconds > 0 ? "timer" : "GPIO only");

    /* RTC GPIO 唤醒 (低电平触发) */
    if (wakeup_pin >= 0) {
        esp_sleep_enable_ext0_wakeup((gpio_num_t)wakeup_pin, 0);
        ESP_LOGI(TAG, "Wakeup on GPIO%d (LOW)", wakeup_pin);
    }

    /* 定时器唤醒 */
    if (seconds > 0) {
        esp_sleep_enable_timer_wakeup((uint64_t)seconds * 1000000ULL);
        ESP_LOGI(TAG, "Wakeup after %lu seconds", (unsigned long)seconds);
    }

    /* 延时让日志输出完成 */
    vTaskDelay(pdMS_TO_TICKS(100));

    /* 进入深度睡眠 */
    esp_deep_sleep_start();
}
