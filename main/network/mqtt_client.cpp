/**
 * @file mqtt_client.cpp
 * @brief ESP-MQTT 客户端实现 (使用 ESP-IDF mqtt/ component)
 */

#include "app_mqtt.h"
#include "config.h"
#include "led_control.h"
#include "sensors.h"
#include "storage.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <mqtt_client.h>

static const char *TAG = "MQTT";

/* ---------- 全局变量 ---------- */
bool g_mqtt_enabled = false;
static esp_mqtt_client_handle_t mqtt_client = NULL;
static bool mqtt_connected = false;

/* ---------- MQTT Broker 配置 ---------- */
/* NOTE: URI 由 mqtt_init() 根据 g_settings.mqttBroker 构建 */

/* ---------- Topic 定义 (使用 config.h 中的 MQTT_TOPIC_*) ---------- */
#define MQTT_TOPIC_STATUS  MQTT_TOPIC_STATE
#define MQTT_TOPIC_SENSORS MQTT_TOPIC_SENSOR

/* ---------- Event Handler ---------- */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connected");
        mqtt_connected = true;
        g_mqtt_enabled = true;
        /* 订阅控制 topic */
        esp_mqtt_client_subscribe(mqtt_client, MQTT_TOPIC_SET, 0);
        /* 发布上线状态 */
        mqtt_publish_status("online");
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT disconnected");
        mqtt_connected = false;
        g_mqtt_enabled = false;
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT subscribed, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT unsubscribed, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGD(TAG, "MQTT published, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT data received: topic=%.*s", event->topic_len, event->topic);
        /* 解析 JSON 命令 */
        {
            char topic[64] = {0};
            char data[256] = {0};
            int tlen = event->topic_len < (int)sizeof(topic) - 1 ? event->topic_len : (int)sizeof(topic) - 1;
            int dlen = event->data_len < (int)sizeof(data) - 1 ? event->data_len : (int)sizeof(data) - 1;
            memcpy(topic, event->topic, tlen);
            memcpy(data, event->data, dlen);

            /* 仅处理 SET topic */
            if (strstr(topic, "set")) {
                int cct = 0, brt = 0;
                if (sscanf(data, "{\"cct\":%d,\"brt\":%d}", &cct, &brt) == 2) {
                    if (cct >= 2700 && cct <= 6500) {
                        led_set_cct((uint16_t)cct);
                    }
                    if (brt >= 0 && brt <= 100) {
                        led_set_brightness((uint8_t)brt);
                    }
                    ESP_LOGI(TAG, "MQTT set: cct=%d brt=%d", cct, brt);
                }
            }
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error");
        break;

    default:
        break;
    }
}

/* ======================== Public API ======================== */

void mqtt_init(void)
{
    if (strlen(g_settings.mqttBroker) == 0) {
        ESP_LOGI(TAG, "MQTT broker not configured, skipping");
        g_mqtt_enabled = false;
        return;
    }

    char uri[256];
    snprintf(uri, sizeof(uri), "mqtt://%s:%d",
             g_settings.mqttBroker, g_settings.mqttPort);

    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = uri;
    mqtt_cfg.credentials.username = g_settings.mqttUser;
    mqtt_cfg.credentials.authentication.password = g_settings.mqttPass;

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "MQTT client init failed");
        return;
    }

    ESP_ERROR_CHECK(esp_mqtt_client_register_event(
        mqtt_client, MQTT_EVENT_ANY, mqtt_event_handler, NULL));

    esp_err_t err = esp_mqtt_client_start(mqtt_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "MQTT start failed: %s", esp_err_to_name(err));
        return;
    }

    g_mqtt_enabled = true;
    ESP_LOGI(TAG, "MQTT init done, uri=%s", uri);
}

void mqtt_loop(void)
{
    /* MQTT 使用事件驱动，不需要主动 poll */
    (void)mqtt_connected;
}

void mqtt_publish_sensors(void)
{
    if (!mqtt_connected || mqtt_client == NULL) return;

    char buf[256];
    snprintf(buf, sizeof(buf),
             "{\"temp\":%.1f,\"lux\":%u,\"i_in\":%.2f,\"i_bat\":%.2f}",
             g_sensors.temperature,
             (unsigned)g_sensors.ldr_lux,
             g_sensors.current_in,
             g_sensors.current_bat);

    esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_SENSORS, buf, 0, 1, 0);
}

void mqtt_publish_status(const char *event)
{
    if (!mqtt_connected || mqtt_client == NULL) return;

    uint8_t mac[6];
    char buf[256];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(buf, sizeof(buf),
             "{\"event\":\"%s\",\"mac\":\"%02X:%02X:%02X:%02X:%02X:%02X\","
             "\"cct\":%u,\"brt\":%u}",
             event,
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
             g_colorTemp, g_brightness);

    esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_STATUS, buf, 0, 1, 0);
}
