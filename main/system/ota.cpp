#include "ota.h"
#include "esp_https_ota.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "OTA";

bool ota_update(const char *url)
{
    ESP_LOGI(TAG, "Starting OTA update from: %s", url);

    esp_http_client_config_t http_cfg = {};
    http_cfg.url = url;
    http_cfg.cert_pem = NULL; /* Skip cert verify for private servers */
    http_cfg.timeout_ms = 30000;

    esp_https_ota_config_t ota_cfg = {};
    ota_cfg.http_config = &http_cfg;

    esp_err_t ret = esp_https_ota(&ota_cfg);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA update success, rebooting...");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
        return true;
    }

    ESP_LOGE(TAG, "OTA update failed: %s", esp_err_to_name(ret));
    return false;
}

const char* ota_get_running_partition(void)
{
    const esp_partition_t *part = esp_ota_get_running_partition();
    return part ? part->label : "unknown";
}
