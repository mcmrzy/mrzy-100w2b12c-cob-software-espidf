#include "lvgl_indev.h"
#include "config.h"
#include "esp_log.h"

static const char *TAG = "LVGL_INDEV";

/* ========== Static Variables ========== */
static lv_indev_drv_t enc1_drv;      ///< Encoder 1 driver
static lv_indev_drv_t enc2_drv;      ///< Encoder 2 driver

static volatile int ec1_diff  = 0;   ///< Accumulated encoder 1 delta
static volatile int ec2_diff  = 0;   ///< Accumulated encoder 2 delta
static volatile bool ec1_pressed = false;  ///< Encoder 1 button state
static volatile bool ec2_pressed = false;  ///< Encoder 2 button state

/* ========== Encoder 1 Read Callback ========== */
static void enc1_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    data->enc_diff = ec1_diff;
    data->state = ec1_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;

    // Reset accumulated delta after reading
    ec1_diff = 0;
}

/* ========== Encoder 2 Read Callback ========== */
static void enc2_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    data->enc_diff = ec2_diff;
    data->state = ec2_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;

    // Reset accumulated delta after reading
    ec2_diff = 0;
}

/* ========== lvgl_indev_init() ========== */
void lvgl_indev_init(void)
{
    // --- Encoder 1 ---
    lv_indev_drv_init(&enc1_drv);
    enc1_drv.type = LV_INDEV_TYPE_ENCODER;
    enc1_drv.read_cb = enc1_read_cb;
    lv_indev_drv_register(&enc1_drv);

    // --- Encoder 2 ---
    lv_indev_drv_init(&enc2_drv);
    enc2_drv.type = LV_INDEV_TYPE_ENCODER;
    enc2_drv.read_cb = enc2_read_cb;
    lv_indev_drv_register(&enc2_drv);

    ESP_LOGI(TAG, "LVGL input devices initialized (2 encoders)");
}

/* ========== lvgl_indev_feed_encoder1() ========== */
void lvgl_indev_feed_encoder1(int delta)
{
    ec1_diff += delta;
}

/* ========== lvgl_indev_feed_encoder2() ========== */
void lvgl_indev_feed_encoder2(int delta)
{
    ec2_diff += delta;
}

/* ========== lvgl_indev_feed_key1() ========== */
void lvgl_indev_feed_key1(bool pressed)
{
    ec1_pressed = pressed;
}

/* ========== lvgl_indev_feed_key2() ========== */
void lvgl_indev_feed_key2(bool pressed)
{
    ec2_pressed = pressed;
}
