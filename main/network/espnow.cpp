#include "espnow.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "ESP-NOW";

static espnow_recv_cb_t user_callback = NULL;
static uint8_t broadcast_mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static void on_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    char mac[18];
    snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
             info->src_addr[0], info->src_addr[1], info->src_addr[2],
             info->src_addr[3], info->src_addr[4], info->src_addr[5]);
    ESP_LOGI(TAG, "Recv %d bytes from %s", len, mac);

    if (user_callback) {
        user_callback((const char*)data, len);
    }
}

static void on_send(const uint8_t *mac, esp_now_send_status_t status)
{
    ESP_LOGD(TAG, "Send to %02X:%02X... %s",
             mac[0], mac[1],
             status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

bool espnow_init(espnow_recv_cb_t callback)
{
    user_callback = callback;

    esp_err_t ret = esp_now_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW init failed: %s", esp_err_to_name(ret));
        return false;
    }

    esp_now_register_recv_cb(on_recv);
    esp_now_register_send_cb(on_send);

    /* Add broadcast peer */
    esp_now_peer_info_t peer = {};
    peer.channel = 0;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    memcpy(peer.peer_addr, broadcast_mac, 6);
    esp_now_add_peer(&peer);

    ESP_LOGI(TAG, "ESP-NOW initialized, broadcasting on ch 0");
    return true;
}

bool espnow_send(const uint8_t *data, int len)
{
    esp_err_t ret = esp_now_send(broadcast_mac, data, len);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "ESP-NOW send failed: %s", esp_err_to_name(ret));
        return false;
    }
    return true;
}
