/**
 * @file ble_ui.cpp
 * @brief NimBLE BLE 控制界面实现
 *
 * ESP-IDF v5.4 NimBLE API
 * GATT 服务: CCT, 亮度, 蜂鸣器频率, 蜂鸣器占空比, 场景, 传感器
 */

extern "C" {

#include "ble_ui.h"
#include "config.h"
#include "storage.h"
#include "led_control.h"
#include "sensors.h"

#include <stdio.h>
#include <string.h>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <nimble/nimble_port.h>
#include <host/ble_hs.h>
#include <host/util/util.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>

#define TAG "BLE_UI"

/* ======================== BLE UUID 定义 ======================== */

/* 服务 UUID: 0xA000 */
static const ble_uuid16_t SVC_UUID =
    BLE_UUID16_INIT(0xA000);

/* Characteristic UUIDs */
static const ble_uuid16_t CCT_UUID    = BLE_UUID16_INIT(0xA001);
static const ble_uuid16_t BRIGHT_UUID = BLE_UUID16_INIT(0xA002);
static const ble_uuid16_t BUZZ_F_UUID = BLE_UUID16_INIT(0xA003);
static const ble_uuid16_t BUZZ_D_UUID = BLE_UUID16_INIT(0xA004);
static const ble_uuid16_t SCENE_UUID  = BLE_UUID16_INIT(0xA005);
static const ble_uuid16_t SENSOR_UUID = BLE_UUID16_INIT(0xA006);

/* ======================== 全局变量 ======================== */

static uint16_t conn_handle = BLE_HS_CONN_HANDLE_NONE;

/* Characteristic value handles (用于发送通知) */
static uint16_t cct_handle;
static uint16_t bright_handle;
static uint16_t buzz_f_handle;
static uint16_t buzz_d_handle;
static uint16_t scene_handle;
static uint16_t sensor_handle;

/* ======================== GATT 访问回调 ======================== */

static int gatt_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    int rc;

    switch (ctxt->op) {

    /* ---- 读操作 ---- */
    case BLE_GATT_ACCESS_OP_READ_CHR:
        if (ble_uuid_cmp(ctxt->chr->uuid, &CCT_UUID.u) == 0) {
            uint16_t val = g_colorTemp;
            rc = os_mbuf_append(ctxt->om, &val, sizeof(val));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        if (ble_uuid_cmp(ctxt->chr->uuid, &BRIGHT_UUID.u) == 0) {
            uint8_t val = g_brightness;
            rc = os_mbuf_append(ctxt->om, &val, sizeof(val));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        if (ble_uuid_cmp(ctxt->chr->uuid, &BUZZ_F_UUID.u) == 0) {
            uint16_t val = 0;
#ifdef STORAGE_H
            val = g_settings.buzzerFreq;
#endif
            rc = os_mbuf_append(ctxt->om, &val, sizeof(val));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        if (ble_uuid_cmp(ctxt->chr->uuid, &BUZZ_D_UUID.u) == 0) {
            uint8_t val = 0;
#ifdef STORAGE_H
            val = g_settings.buzzerDuty;
#endif
            rc = os_mbuf_append(ctxt->om, &val, sizeof(val));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        if (ble_uuid_cmp(ctxt->chr->uuid, &SCENE_UUID.u) == 0) {
            uint8_t val = 0;
            rc = os_mbuf_append(ctxt->om, &val, sizeof(val));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        if (ble_uuid_cmp(ctxt->chr->uuid, &SENSOR_UUID.u) == 0) {
            /* 传感器数据以 JSON 格式返回 */
            char json[128];
            snprintf(json, sizeof(json),
                     "{\"bus\":%.1f,\"bat\":%.1f,\"temp\":%.1f,\"ldr\":%u}",
                     g_sensors.bus_voltage, g_sensors.bat_voltage,
                     g_sensors.temperature, g_sensors.ldr_lux);
            rc = os_mbuf_append(ctxt->om, json, strlen(json));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        return BLE_ATT_ERR_UNLIKELY;

    /* ---- 写操作 ---- */
    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        if (ble_uuid_cmp(ctxt->chr->uuid, &CCT_UUID.u) == 0) {
            uint16_t val;
            if (os_mbuf_copydata(ctxt->om, 0, sizeof(val), &val) == 0) {
                if (val >= CCT_MIN && val <= CCT_MAX) {
                    led_set_cct(val);
                    ESP_LOGI(TAG, "BLE set CCT: %d", val);
                }
            }
            return 0;
        }
        if (ble_uuid_cmp(ctxt->chr->uuid, &BRIGHT_UUID.u) == 0) {
            uint8_t val;
            if (os_mbuf_copydata(ctxt->om, 0, sizeof(val), &val) == 0) {
                if (val <= BRT_MAX) {
                    led_set_brightness(val);
                    ESP_LOGI(TAG, "BLE set Brightness: %d", val);
                }
            }
            return 0;
        }
        if (ble_uuid_cmp(ctxt->chr->uuid, &BUZZ_F_UUID.u) == 0) {
            uint16_t val;
            if (os_mbuf_copydata(ctxt->om, 0, sizeof(val), &val) == 0) {
                if (val >= BUZZER_MIN_FREQ && val <= BUZZER_MAX_FREQ) {
#ifdef STORAGE_H
                    g_settings.buzzerFreq = val;
#endif
                    ESP_LOGI(TAG, "BLE set Buzzer Freq: %d", val);
                }
            }
            return 0;
        }
        if (ble_uuid_cmp(ctxt->chr->uuid, &BUZZ_D_UUID.u) == 0) {
            uint8_t val;
            if (os_mbuf_copydata(ctxt->om, 0, sizeof(val), &val) == 0) {
                if (val >= BUZZER_MIN_DUTY && val <= BUZZER_MAX_DUTY) {
#ifdef STORAGE_H
                    g_settings.buzzerDuty = val;
#endif
                    ESP_LOGI(TAG, "BLE set Buzzer Duty: %d", val);
                }
            }
            return 0;
        }
        if (ble_uuid_cmp(ctxt->chr->uuid, &SCENE_UUID.u) == 0) {
            uint8_t val;
            if (os_mbuf_copydata(ctxt->om, 0, sizeof(val), &val) == 0) {
                if (val < SCENE_COUNT) {
                    led_set_cct(SCENES[val][0]);
                    led_set_brightness((uint8_t)SCENES[val][1]);
                    ESP_LOGI(TAG, "BLE set Scene %d: CCT=%d, BRI=%d",
                             val, SCENES[val][0], SCENES[val][1]);
                }
            }
            return 0;
        }
        return BLE_ATT_ERR_UNLIKELY;

    default:
        return BLE_ATT_ERR_UNLIKELY;
    }
}

/* ======================== GATT 服务表 ======================== */

/*
 * ESP-IDF v5.4 ble_gatt_chr_def 字段顺序:
 *   uuid, access_cb, arg, descriptors, flags, min_key_size, val_handle, cpfd
 */
static const struct ble_gatt_svc_def gatt_services[] = {
    {
        /* 主服务 */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &SVC_UUID.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                /* CCT (0xA001) - 读写 + 通知 */
                .uuid = &CCT_UUID.u,
                .access_cb = gatt_chr_access,
                .arg = NULL,
                .descriptors = NULL,
                .flags = BLE_GATT_CHR_F_READ |
                         BLE_GATT_CHR_F_WRITE |
                         BLE_GATT_CHR_F_NOTIFY,
                .min_key_size = 0,
                .val_handle = &cct_handle,
                .cpfd = NULL,
            },
            {
                /* 亮度 (0xA002) - 读写 + 通知 */
                .uuid = &BRIGHT_UUID.u,
                .access_cb = gatt_chr_access,
                .arg = NULL,
                .descriptors = NULL,
                .flags = BLE_GATT_CHR_F_READ |
                         BLE_GATT_CHR_F_WRITE |
                         BLE_GATT_CHR_F_NOTIFY,
                .min_key_size = 0,
                .val_handle = &bright_handle,
                .cpfd = NULL,
            },
            {
                /* 蜂鸣器频率 (0xA003) - 读写 */
                .uuid = &BUZZ_F_UUID.u,
                .access_cb = gatt_chr_access,
                .arg = NULL,
                .descriptors = NULL,
                .flags = BLE_GATT_CHR_F_READ |
                         BLE_GATT_CHR_F_WRITE,
                .min_key_size = 0,
                .val_handle = &buzz_f_handle,
                .cpfd = NULL,
            },
            {
                /* 蜂鸣器占空比 (0xA004) - 读写 */
                .uuid = &BUZZ_D_UUID.u,
                .access_cb = gatt_chr_access,
                .arg = NULL,
                .descriptors = NULL,
                .flags = BLE_GATT_CHR_F_READ |
                         BLE_GATT_CHR_F_WRITE,
                .min_key_size = 0,
                .val_handle = &buzz_d_handle,
                .cpfd = NULL,
            },
            {
                /* 场景 (0xA005) - 写 + 通知 */
                .uuid = &SCENE_UUID.u,
                .access_cb = gatt_chr_access,
                .arg = NULL,
                .descriptors = NULL,
                .flags = BLE_GATT_CHR_F_READ |
                         BLE_GATT_CHR_F_WRITE |
                         BLE_GATT_CHR_F_NOTIFY,
                .min_key_size = 0,
                .val_handle = &scene_handle,
                .cpfd = NULL,
            },
            {
                /* 传感器 (0xA006) - 只读 + 通知 */
                .uuid = &SENSOR_UUID.u,
                .access_cb = gatt_chr_access,
                .arg = NULL,
                .descriptors = NULL,
                .flags = BLE_GATT_CHR_F_READ |
                         BLE_GATT_CHR_F_NOTIFY,
                .min_key_size = 0,
                .val_handle = &sensor_handle,
                .cpfd = NULL,
            },
            {
                /* 特征结束标记 */
                .uuid = NULL,
                .access_cb = NULL,
                .arg = NULL,
                .descriptors = NULL,
                .flags = 0,
                .min_key_size = 0,
                .val_handle = NULL,
                .cpfd = NULL,
            },
        },
    },
    {
        /* 服务表结束标记 */
        .type = 0,
        .uuid = NULL,
        .includes = NULL,
        .characteristics = NULL,
    }
};

/* ======================== 广播 (前向声明) ======================== */
static void ble_advertise(void);

/* ======================== GAP 事件处理 ======================== */

static int gap_event_handler(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            conn_handle = event->connect.conn_handle;
            ESP_LOGI(TAG, "BLE client connected, handle=%d", conn_handle);
        } else {
            ESP_LOGW(TAG, "BLE connection failed, status=%d",
                     event->connect.status);
            /* 连接失败, 重新开始广播 */
            ble_advertise();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        /* v5.4: 使用 event->disconnect.conn.conn_handle */
        ESP_LOGI(TAG, "BLE client disconnected, handle=%d, reason=%d",
                 event->disconnect.conn.conn_handle,
                 event->disconnect.reason);
        conn_handle = BLE_HS_CONN_HANDLE_NONE;
        /* 断开后重新广播 */
        ble_advertise();
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "BLE advertising complete");
        ble_advertise();
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI(TAG, "BLE subscribe: cur_notify=%d, cur_indicate=%d",
                 event->subscribe.cur_notify,
                 event->subscribe.cur_indicate);
        return 0;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "BLE MTU updated: %d", event->mtu.value);
        return 0;

    default:
        return 0;
    }
}

/* ======================== 广播 ======================== */

static void ble_advertise(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;

    memset(&fields, 0, sizeof(fields));

    /* 标志: 通用可发现 + 支持 BLE */
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    /* 完整设备名 */
    fields.name = (uint8_t *)BLE_DEVICE_NAME;
    fields.name_len = strlen(BLE_DEVICE_NAME);
    fields.name_is_complete = 1;

    /* 设置广播数据 */
    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_set_fields failed: %d", rc);
        return;
    }

    /* 扫描响应数据 */
    memset(&fields, 0, sizeof(fields));
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    rc = ble_gap_adv_rsp_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_rsp_set_fields failed: %d", rc);
        return;
    }

    /* 广播参数: 可连接可发现, 通用发现模式 */
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.itvl_min = BLE_GAP_ADV_FAST_INTERVAL1_MIN;
    adv_params.itvl_max = BLE_GAP_ADV_FAST_INTERVAL1_MAX;

    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                           &adv_params, gap_event_handler, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_start failed: %d", rc);
        return;
    }

    ESP_LOGI(TAG, "BLE advertising started");
}

/* ======================== NimBLE 同步回调 ======================== */

static void ble_on_sync(void)
{
    ESP_LOGI(TAG, "BLE host synced");

    /* 确保有有效的 BLE 地址 */
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_hs_util_ensure_addr failed: %d", rc);
        return;
    }

    /* 开始广播 */
    ble_advertise();
}

/* ======================== NimBLE 主机任务 ======================== */

static void ble_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE host task started");

    /* 初始化 NimBLE port */
    int rc = nimble_port_init();
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %d", rc);
        vTaskDelete(NULL);
        return;
    }

    /* 初始化 NimBLE host */
    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.gatts_register_cb = NULL;  /* v5.4: 注册回调设为 NULL */

    /* 注册 GATT 服务 */
    rc = ble_gatts_count_cfg(gatt_services);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_count_cfg failed: %d", rc);
        vTaskDelete(NULL);
        return;
    }

    rc = ble_gatts_add_svcs(gatt_services);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_add_svcs failed: %d", rc);
        vTaskDelete(NULL);
        return;
    }

    /* 注册 GAP/GATT 服务 */
    ble_svc_gap_init();
    ble_svc_gatt_init();

    /* 设置设备名 */
    ble_svc_gap_device_name_set(BLE_DEVICE_NAME);

    /* 运行 NimBLE host 任务 (阻塞) */
    nimble_port_run();

    /* 正常不会到达这里 */
    nimble_port_deinit();
    vTaskDelete(NULL);
}

/* ======================== BLE 初始化 ======================== */

void ble_init(void)
{
    ESP_LOGI(TAG, "Initializing BLE...");

    /* 创建 BLE 主机任务 */
    xTaskCreate(ble_host_task, "ble_host", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "BLE init done, device name: %s", BLE_DEVICE_NAME);
}

/* ======================== BLE 通知 ======================== */

void ble_notify(void)
{
    if (conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        return;
    }

    struct os_mbuf *om;

    /* 通知 CCT 值 */
    om = ble_hs_mbuf_from_flat(&g_colorTemp, sizeof(g_colorTemp));
    if (om) {
        ble_gatts_notify_custom(conn_handle, cct_handle, om);
    }

    /* 通知亮度值 */
    om = ble_hs_mbuf_from_flat(&g_brightness, sizeof(g_brightness));
    if (om) {
        ble_gatts_notify_custom(conn_handle, bright_handle, om);
    }

    /* 通知传感器数据 (JSON) */
    char json[128];
    snprintf(json, sizeof(json),
             "{\"bus\":%.1f,\"bat\":%.1f,\"temp\":%.1f,\"ldr\":%u}",
             g_sensors.bus_voltage, g_sensors.bat_voltage,
             g_sensors.temperature, g_sensors.ldr_lux);
    om = ble_hs_mbuf_from_flat(json, strlen(json));
    if (om) {
        ble_gatts_notify_custom(conn_handle, sensor_handle, om);
    }
}

} /* extern "C" */
