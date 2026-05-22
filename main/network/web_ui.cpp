/**
 * @file web_ui.cpp
 * @brief WiFi + HTTP REST API 控制界面实现
 *
 * AP+STA 模式, mDNS, HTTP REST API (无 WebSocket)
 */

extern "C" {

#include "web_ui.h"
#include "config.h"
#include "storage.h"
#include "led_control.h"
#include "sensors.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_http_server.h>
#include <esp_ota_ops.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_mac.h>

#include <cJSON.h>
#include <mdns.h>

#define TAG "WEB_UI"

/* ======================== WiFi 事件处理 ======================== */

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
        case WIFI_EVENT_AP_START:
            ESP_LOGI(TAG, "AP started, SSID: %s", WIFI_AP_SSID);
            break;
        case WIFI_EVENT_AP_STACONNECTED: {
            wifi_event_ap_staconnected_t *e =
                (wifi_event_ap_staconnected_t *)event_data;
            ESP_LOGI(TAG, "Station connected, MAC: " MACSTR,
                     MAC2STR(e->mac));
            break;
        }
        case WIFI_EVENT_AP_STADISCONNECTED: {
            wifi_event_ap_stadisconnected_t *e =
                (wifi_event_ap_stadisconnected_t *)event_data;
            ESP_LOGI(TAG, "Station disconnected, MAC: " MACSTR,
                     MAC2STR(e->mac));
            break;
        }
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "STA started, connecting...");
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "STA connected to AP");
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGW(TAG, "STA disconnected, retrying...");
            esp_wifi_connect();
            break;
        default:
            break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t *e = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG, "STA got IP: " IPSTR, IP2STR(&e->ip_info.ip));
        }
    }
}

void wifi_init(void)
{
    ESP_LOGI(TAG, "Initializing WiFi (AP+STA)...");

    /* 初始化默认 netif */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* 创建 AP 和 STA netif */
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    /* WiFi 初始化配置 */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* 注册事件 */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    /* AP 配置 */
    wifi_config_t ap_config = {};
    memset(&ap_config.ap, 0, sizeof(ap_config.ap));
    memcpy(ap_config.ap.ssid, WIFI_AP_SSID, strlen(WIFI_AP_SSID));
    ap_config.ap.ssid_len = strlen(WIFI_AP_SSID);
    memcpy(ap_config.ap.password, WIFI_AP_PASS, strlen(WIFI_AP_PASS));
    ap_config.ap.channel = 1;
    ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    ap_config.ap.ssid_hidden = 0;
    ap_config.ap.max_connection = 4;
    if (strlen(WIFI_AP_PASS) == 0) {
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    /* STA 配置 (默认空, 后续可通过 storage 加载) */
    wifi_config_t sta_config = {};
    memset(&sta_config.sta, 0, sizeof(sta_config.sta));
    sta_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    sta_config.sta.pmf_cfg.capable = true;
    sta_config.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi init done. AP SSID: %s, PASS: %s",
             WIFI_AP_SSID, WIFI_AP_PASS);
}

/* ======================== JSON 构建 ======================== */

static char *build_state_json(void)
{
    cJSON *root = cJSON_CreateObject();

    cJSON_AddNumberToObject(root, "cct", g_colorTemp);
    cJSON_AddNumberToObject(root, "bri", g_brightness);
    cJSON_AddNumberToObject(root, "temp", g_sensors.temperature);
    cJSON_AddNumberToObject(root, "bus", g_sensors.bus_voltage);
    cJSON_AddNumberToObject(root, "bat", g_sensors.bat_voltage);
    cJSON_AddNumberToObject(root, "ldr", g_sensors.ldr_lux);

    /* 蜂鸣器参数 - 从 g_settings 读取 */
    cJSON_AddNumberToObject(root, "buzzer_freq", g_settings.buzzerFreq);
    cJSON_AddNumberToObject(root, "buzzer_duty", g_settings.buzzerDuty);

    cJSON_AddBoolToObject(root, "auto", g_auto_brightness);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}

/* ======================== HTTP 处理函数 ======================== */

/* 嵌入式 HTML 控制页面 */
static const char INDEX_HTML[] =
"<!DOCTYPE html>"
"<html><head><meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>COB-LED Control</title>"
"<style>"
"*{margin:0;padding:0;box-sizing:border-box}"
"body{font-family:sans-serif;background:#1a1a2e;color:#eee;max-width:480px;"
"margin:0 auto;padding:16px}"
"h1{text-align:center;font-size:1.4em;margin-bottom:16px;color:#e94560}"
".card{background:#16213e;border-radius:12px;padding:16px;margin-bottom:12px}"
"label{display:block;font-size:0.9em;margin-bottom:4px;color:#aaa}"
"input[type=range]{width:100%;margin:8px 0;accent-color:#e94560}"
".val{text-align:right;font-size:1.1em;font-weight:bold;color:#e94560}"
".scenes{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:8px}"
".sbtn{padding:12px;border:none;border-radius:8px;cursor:pointer;"
"font-size:0.9em;font-weight:bold;color:#fff;transition:opacity .2s}"
".sbtn:active{opacity:0.7}"
".s1{background:#ff6b35}.s2{background:#f7c948}.s3{background:#48bb78}"
".s4{background:#4299e1}"
".sensors{display:grid;grid-template-columns:1fr 1fr;gap:6px;margin-top:8px}"
".si{font-size:0.85em;color:#aaa}"
".si span{color:#e94560;font-weight:bold}"
"</style></head><body>"
"<h1>COB-LED Control</h1>"

/* 色温 */
"<div class='card'>"
"<label>Color Temperature</label>"
"<input type='range' id='cct' min='2700' max='6500' step='100' value='4600'>"
"<div class='val'><span id='cctV'>4600</span> K</div>"
"</div>"

/* 亮度 */
"<div class='card'>"
"<label>Brightness</label>"
"<input type='range' id='bri' min='0' max='100' step='1' value='0'>"
"<div class='val'><span id='briV'>0</span> %</div>"
"</div>"

/* 场景 */
"<div class='card'>"
"<label>Scenes</label>"
"<div class='scenes'>"
"<button class='sbtn s1' onclick='scene(0)'>Warm 2700K</button>"
"<button class='sbtn s2' onclick='scene(1)'>Neutral 3500K</button>"
"<button class='sbtn s3' onclick='scene(2)'>Daylight 4600K</button>"
"<button class='sbtn s4' onclick='scene(3)'>Cool 6500K</button>"
"</div></div>"

/* 传感器 */
"<div class='card'>"
"<label>Sensors</label>"
"<div class='sensors'>"
"<div class='si'>Bus: <span id='sBus'>--</span> V</div>"
"<div class='si'>Bat: <span id='sBat'>--</span> V</div>"
"<div class='si'>Temp: <span id='sTemp'>--</span> C</div>"
"<div class='si'>Lux: <span id='sLdr'>--</span></div>"
"</div></div>"

"<script>"
"var last=0;"
"function f(url,cb){var x=new XMLHttpRequest();x.open('GET',url);"
"x.onload=function(){if(cb)cb(JSON.parse(x.responseText))};x.send()}"
"function set(k,v){if(Date.now()-last<150)return;last=Date.now();"
"var x=new XMLHttpRequest();x.open('POST','/api/set');"
"x.setRequestHeader('Content-Type','application/json');"
"x.send(JSON.stringify({[k]:v}))}"
"function scene(i){if(Date.now()-last<150)return;last=Date.now();"
"var x=new XMLHttpRequest();x.open('POST','/api/scene');"
"x.setRequestHeader('Content-Type','application/json');"
"x.send(JSON.stringify({scene:i}))}"
"document.getElementById('cct').oninput=function(){"
"document.getElementById('cctV').textContent=this.value;set('cct',+this.value)};"
"document.getElementById('bri').oninput=function(){"
"document.getElementById('briV').textContent=this.value;set('bri',+this.value)};"
"function poll(){f('/api/state',function(d){"
"document.getElementById('cct').value=d.cct;"
"document.getElementById('cctV').textContent=d.cct;"
"document.getElementById('bri').value=d.bri;"
"document.getElementById('briV').textContent=d.bri;"
"document.getElementById('sBus').textContent=d.bus.toFixed(1);"
"document.getElementById('sBat').textContent=d.bat.toFixed(1);"
"document.getElementById('sTemp').textContent=d.temp.toFixed(1);"
"document.getElementById('sLdr').textContent=d.ldr;"
"})}"
"setInterval(poll,1000);poll();"
"</script></body></html>";

/* GET / - 返回嵌入式 HTML */
static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
}

/* GET /api/state - 返回当前状态 JSON */
static esp_err_t state_handler(httpd_req_t *req)
{
    char *json = build_state_json();
    if (!json) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, json, strlen(json));
    free(json);
    return ret;
}

/* POST /api/set - 设置参数 (cct, bri, auto) */
static esp_err_t set_handler(httpd_req_t *req)
{
    char buf[256] = {0};
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *cct = cJSON_GetObjectItem(root, "cct");
    cJSON *bri = cJSON_GetObjectItem(root, "bri");
    cJSON *auto_bri = cJSON_GetObjectItem(root, "auto");

    if (cct && cJSON_IsNumber(cct)) {
        uint16_t val = (uint16_t)cct->valueint;
        if (val >= CCT_MIN && val <= CCT_MAX) {
            led_set_cct(val);
        }
    }

    if (bri && cJSON_IsNumber(bri)) {
        uint8_t val = (uint8_t)bri->valueint;
        if (val <= BRT_MAX) {
            led_set_brightness(val);
        }
    }

    if (auto_bri && cJSON_IsBool(auto_bri)) {
        g_auto_brightness = cJSON_IsTrue(auto_bri);
    }

    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    const char *resp = "{\"status\":\"ok\"}";
    return httpd_resp_send(req, resp, strlen(resp));
}

/* POST /api/scene - 切换场景 */
static esp_err_t scene_handler(httpd_req_t *req)
{
    char buf[128] = {0};
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *scene = cJSON_GetObjectItem(root, "scene");
    if (scene && cJSON_IsNumber(scene)) {
        int idx = scene->valueint;
        if (idx >= 0 && idx < SCENE_COUNT) {
            led_set_cct(SCENES[idx][0]);
            led_set_brightness((uint8_t)SCENES[idx][1]);
            ESP_LOGI(TAG, "Scene %d: CCT=%d, BRI=%d",
                     idx, SCENES[idx][0], SCENES[idx][1]);
        }
    }

    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    const char *resp = "{\"status\":\"ok\"}";
    return httpd_resp_send(req, resp, strlen(resp));
}

/* POST /api/update - OTA 固件更新 */
static esp_err_t update_handler(httpd_req_t *req)
{
    esp_ota_handle_t ota_handle = 0;
    const esp_partition_t *update_partition = NULL;

    update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        ESP_LOGE(TAG, "No OTA partition found");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "No OTA partition");
        return ESP_FAIL;
    }

    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN,
                                  &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA begin failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "OTA begin failed");
        return ESP_FAIL;
    }

    char buf[1024];
    int remaining = req->content_len;
    int received = 0;

    while (remaining > 0) {
        int len = httpd_req_recv(req, buf, sizeof(buf));
        if (len <= 0) {
            if (len == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            ESP_LOGE(TAG, "OTA recv error");
            esp_ota_end(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                "OTA recv error");
            return ESP_FAIL;
        }

        err = esp_ota_write(ota_handle, buf, len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "OTA write failed: %s", esp_err_to_name(err));
            esp_ota_end(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                "OTA write failed");
            return ESP_FAIL;
        }

        remaining -= len;
        received += len;
    }

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA end failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "OTA end failed");
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA set boot failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "OTA set boot failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA update success, received %d bytes, rebooting...",
             received);

    httpd_resp_set_type(req, "application/json");
    const char *resp = "{\"status\":\"ok\",\"msg\":\"OTA success, rebooting\"}";
    httpd_resp_send(req, resp, strlen(resp));

    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();

    return ESP_OK;
}

/* ======================== HTTP 服务器初始化 ======================== */

void webui_init(void)
{
    ESP_LOGI(TAG, "Initializing Web UI...");

    /* 启动 mDNS */
    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set(WIFI_HOSTNAME));
    ESP_ERROR_CHECK(mdns_instance_name_set("COB-LED Controller"));

    /* 添加 HTTP 服务 */
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);

    ESP_LOGI(TAG, "mDNS started: http://%s.local", WIFI_HOSTNAME);

    /* 创建 HTTP 服务器 */
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8;
    config.stack_size = 8192;

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return;
    }

    /* 注册 URI 处理 */
    httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = index_handler,
        .user_ctx = NULL,
    };
    httpd_uri_t state_uri = {
        .uri = "/api/state",
        .method = HTTP_GET,
        .handler = state_handler,
        .user_ctx = NULL,
    };
    httpd_uri_t set_uri = {
        .uri = "/api/set",
        .method = HTTP_POST,
        .handler = set_handler,
        .user_ctx = NULL,
    };
    httpd_uri_t scene_uri = {
        .uri = "/api/scene",
        .method = HTTP_POST,
        .handler = scene_handler,
        .user_ctx = NULL,
    };
    httpd_uri_t update_uri = {
        .uri = "/api/update",
        .method = HTTP_POST,
        .handler = update_handler,
        .user_ctx = NULL,
    };

    httpd_register_uri_handler(server, &index_uri);
    httpd_register_uri_handler(server, &state_uri);
    httpd_register_uri_handler(server, &set_uri);
    httpd_register_uri_handler(server, &scene_uri);
    httpd_register_uri_handler(server, &update_uri);

    ESP_LOGI(TAG, "HTTP server started on port %d", config.server_port);
}

} /* extern "C" */
