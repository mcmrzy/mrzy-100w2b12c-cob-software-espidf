/**
 * @file storage.cpp
 * @brief NVS 持久化存储实现 (带 CRC 校验)
 *
 * 使用 ESP-IDF NVS API 实现用户设置的读写。
 * 命名空间: "cob-led"
 * 关键参数带 CRC32 校验, 防止掉电写坏数据。
 */

#include "storage.h"
#include "config.h"

#include <cstring>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_rom_crc.h"

static const char *TAG = "storage";

/* NVS 命名空间 */
static const char *NVS_NAMESPACE = "cob-led";

/* NVS 键名 */
static const char *KEY_CCT      = "cct";
static const char *KEY_BRT      = "brt";
static const char *KEY_BZ_FREQ  = "bzFreq";
static const char *KEY_BZ_DUTY  = "bzDuty";
static const char *KEY_AUTO_BRT = "autoBrt";
static const char *KEY_MQTT_EN  = "mqttEn";
static const char *KEY_MQTT_BR  = "mqttBr";
static const char *KEY_MQTT_PORT = "mqttPt";
static const char *KEY_MQTT_USER = "mqttUsr";
static const char *KEY_MQTT_PASS = "mqttPwd";
static const char *KEY_CRC      = "crc32";

/* ======================== 全局设置实例 ======================== */

Settings g_settings = {
    .colorTemp   = CCT_DEFAULT,
    .brightness  = BRT_DEFAULT,
    .buzzerFreq  = BUZZER_MIN_FREQ,
    .buzzerDuty  = 32,
    .autoBright  = false,
    .mqttEnable  = false,
    .mqttBroker  = "",
    .mqttPort    = MQTT_PORT,
    .mqttUser    = "",
    .mqttPass    = "",
};

/* ======================== CRC 计算 ======================== */

/* 对关键参数计算 CRC32 (不含字符串字段) */
static uint32_t calc_settings_crc(const Settings &s)
{
    struct {
        uint16_t colorTemp;
        uint8_t  brightness;
        uint16_t buzzerFreq;
        uint8_t  buzzerDuty;
        uint8_t  autoBright;
        uint8_t  mqttEnable;
        uint16_t mqttPort;
    } pack = {
        s.colorTemp,
        s.brightness,
        s.buzzerFreq,
        s.buzzerDuty,
        (uint8_t)(s.autoBright ? 1 : 0),
        (uint8_t)(s.mqttEnable ? 1 : 0),
        s.mqttPort,
    };
    return esp_rom_crc32_le(0, (const uint8_t *)&pack, sizeof(pack));
}

/* ======================== 内部辅助函数 ======================== */

static bool nvs_open_namespace(nvs_handle_t *handle)
{
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

/* ======================== 公共接口 ======================== */

void storage_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition needs erase, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS flash init failed: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "NVS initialized, loading settings...");
    storage_load(g_settings);
}

void storage_load(Settings &s)
{
    nvs_handle_t handle;
    if (!nvs_open_namespace(&handle)) {
        return;
    }

    /* 读取色温 (u16) */
    uint16_t cct = s.colorTemp;
    if (nvs_get_u16(handle, KEY_CCT, &cct) == ESP_OK) {
        s.colorTemp = cct;
    }
    if (s.colorTemp < CCT_MIN) s.colorTemp = CCT_MIN;
    if (s.colorTemp > CCT_MAX) s.colorTemp = CCT_MAX;

    /* 读取亮度 (u8) */
    uint8_t brt = s.brightness;
    if (nvs_get_u8(handle, KEY_BRT, &brt) == ESP_OK) {
        s.brightness = brt;
    }
    if (s.brightness > BRT_MAX) s.brightness = BRT_MAX;

    /* 读取蜂鸣器频率 (u16) */
    uint16_t bzFreq = s.buzzerFreq;
    if (nvs_get_u16(handle, KEY_BZ_FREQ, &bzFreq) == ESP_OK) {
        s.buzzerFreq = bzFreq;
    }
    if (s.buzzerFreq < BUZZER_MIN_FREQ) s.buzzerFreq = BUZZER_MIN_FREQ;
    if (s.buzzerFreq > BUZZER_MAX_FREQ) s.buzzerFreq = BUZZER_MAX_FREQ;

    /* 读取蜂鸣器占空比 (u8) */
    uint8_t bzDuty = s.buzzerDuty;
    if (nvs_get_u8(handle, KEY_BZ_DUTY, &bzDuty) == ESP_OK) {
        s.buzzerDuty = bzDuty;
    }
    if (s.buzzerDuty > BUZZER_MAX_DUTY) s.buzzerDuty = BUZZER_MAX_DUTY;

    /* 读取自动亮度 (i8) */
    int8_t autoBrt = s.autoBright ? 1 : 0;
    if (nvs_get_i8(handle, KEY_AUTO_BRT, &autoBrt) == ESP_OK) {
        s.autoBright = (autoBrt != 0);
    }

    /* 读取 MQTT 使能 (i8) */
    int8_t mqttEn = s.mqttEnable ? 1 : 0;
    if (nvs_get_i8(handle, KEY_MQTT_EN, &mqttEn) == ESP_OK) {
        s.mqttEnable = (mqttEn != 0);
    }

    /* 读取 MQTT Broker */
    size_t len = sizeof(s.mqttBroker);
    nvs_get_str(handle, KEY_MQTT_BR, s.mqttBroker, &len);

    /* 读取 MQTT 端口 */
    uint16_t mqttPort = s.mqttPort;
    if (nvs_get_u16(handle, KEY_MQTT_PORT, &mqttPort) == ESP_OK) {
        s.mqttPort = mqttPort;
    }

    /* 读取 MQTT 用户名 */
    len = sizeof(s.mqttUser);
    nvs_get_str(handle, KEY_MQTT_USER, s.mqttUser, &len);

    /* 读取 MQTT 密码 */
    len = sizeof(s.mqttPass);
    nvs_get_str(handle, KEY_MQTT_PASS, s.mqttPass, &len);

    /* ---- CRC 校验 ---- */
    uint32_t stored_crc = 0;
    if (nvs_get_u32(handle, KEY_CRC, &stored_crc) == ESP_OK) {
        uint32_t computed = calc_settings_crc(s);
        if (stored_crc != computed) {
            ESP_LOGW(TAG, "NVS CRC mismatch! stored=0x%08lX computed=0x%08lX, resetting to defaults",
                     (unsigned long)stored_crc, (unsigned long)computed);
            /* 恢复默认值 */
            s.colorTemp  = CCT_DEFAULT;
            s.brightness = BRT_DEFAULT;
            s.buzzerFreq = BUZZER_MIN_FREQ;
            s.buzzerDuty = 32;
            s.autoBright = false;
            s.mqttEnable = false;
            s.mqttPort   = MQTT_PORT;
            /* 保存正确的 CRC */
            nvs_set_u32(handle, KEY_CRC, calc_settings_crc(s));
            nvs_commit(handle);
        }
    } else {
        /* 首次运行, 存储 CRC */
        nvs_set_u32(handle, KEY_CRC, calc_settings_crc(s));
        nvs_commit(handle);
    }

    nvs_close(handle);

    ESP_LOGI(TAG, "Settings loaded: CCT=%u, BRT=%u, BzFreq=%u, BzDuty=%u, AutoBrt=%d, MqttEn=%d",
             s.colorTemp, s.brightness, s.buzzerFreq, s.buzzerDuty,
             (int)s.autoBright, (int)s.mqttEnable);
}

void storage_save(const Settings &s)
{
    nvs_handle_t handle;
    if (!nvs_open_namespace(&handle)) {
        return;
    }

    esp_err_t err;

    err = nvs_set_u16(handle, KEY_CCT, s.colorTemp);
    if (err != ESP_OK) ESP_LOGE(TAG, "Failed to save CCT: %s", esp_err_to_name(err));

    err = nvs_set_u8(handle, KEY_BRT, s.brightness);
    if (err != ESP_OK) ESP_LOGE(TAG, "Failed to save brightness: %s", esp_err_to_name(err));

    err = nvs_set_u16(handle, KEY_BZ_FREQ, s.buzzerFreq);
    if (err != ESP_OK) ESP_LOGE(TAG, "Failed to save buzzer freq: %s", esp_err_to_name(err));

    err = nvs_set_u8(handle, KEY_BZ_DUTY, s.buzzerDuty);
    if (err != ESP_OK) ESP_LOGE(TAG, "Failed to save buzzer duty: %s", esp_err_to_name(err));

    err = nvs_set_i8(handle, KEY_AUTO_BRT, s.autoBright ? 1 : 0);
    if (err != ESP_OK) ESP_LOGE(TAG, "Failed to save auto bright: %s", esp_err_to_name(err));

    err = nvs_set_i8(handle, KEY_MQTT_EN, s.mqttEnable ? 1 : 0);
    if (err != ESP_OK) ESP_LOGE(TAG, "Failed to save MQTT enable: %s", esp_err_to_name(err));

    if (s.mqttBroker[0] != '\0') nvs_set_str(handle, KEY_MQTT_BR, s.mqttBroker);
    nvs_set_u16(handle, KEY_MQTT_PORT, s.mqttPort);
    if (s.mqttUser[0] != '\0') nvs_set_str(handle, KEY_MQTT_USER, s.mqttUser);
    if (s.mqttPass[0] != '\0') nvs_set_str(handle, KEY_MQTT_PASS, s.mqttPass);

    /* 写入 CRC */
    nvs_set_u32(handle, KEY_CRC, calc_settings_crc(s));

    err = nvs_commit(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS commit failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Settings saved (CRC ok): CCT=%u, BRT=%u", s.colorTemp, s.brightness);
    }

    nvs_close(handle);
}
