/**
 * @file storage.h
 * @brief NVS 持久化存储 - Settings 结构体及接口声明
 *
 * 使用 ESP-IDF NVS (Non-Volatile Storage) 保存用户设置。
 * 命名空间: "cob-led"
 */

#ifndef STORAGE_H
#define STORAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

/* ======================== Settings 结构体 ======================== */

/**
 * @brief 用户设置结构体
 *
 * 所有可持久化的用户偏好参数。
 * 默认值与 config.h 中的宏定义保持一致。
 */
typedef struct {
    uint16_t colorTemp;       /**< 色温 (2700 ~ 6500 K), 默认 CCT_DEFAULT */
    uint8_t  brightness;      /**< 亮度 (0 ~ 100 %), 默认 BRT_DEFAULT */
    uint16_t buzzerFreq;      /**< 蜂鸣器频率 (Hz), 默认 BUZZER_MIN_FREQ */
    uint8_t  buzzerDuty;      /**< 蜂鸣器占空比, 默认 32 */
    bool     autoBright;      /**< 自动亮度开关, 默认 false */
    bool     mqttEnable;      /**< MQTT 使能开关, 默认 false */
    char     mqttBroker[64];  /**< MQTT Broker 地址 */
    uint16_t mqttPort;        /**< MQTT Broker 端口, 默认 1883 */
    char     mqttUser[32];    /**< MQTT 用户名 */
    char     mqttPass[64];    /**< MQTT 密码 */
} Settings;

/* ======================== 全局变量 ======================== */

/** @brief 全局设置实例 */
extern Settings g_settings;

/* ======================== 接口函数 ======================== */

/**
 * @brief 初始化 NVS 并加载默认设置
 *
 * 应在 app_main() 中最早调用。
 * 初始化 NVS flash，加载已保存的设置到 g_settings。
 * 若 NVS 中无有效数据，则使用默认值。
 */
void storage_init(void);

/**
 * @brief 从 NVS 加载设置到指定结构体
 * @param s 目标 Settings 结构体引用
 *
 * 逐项从 NVS 读取，缺失的键使用默认值填充。
 */
void storage_load(Settings &s);

/**
 * @brief 将设置保存到 NVS
 * @param s 源 Settings 结构体引用
 *
 * 逐项写入 NVS，立即 commit。
 */
void storage_save(const Settings &s);

#ifdef __cplusplus
}
#endif

#endif /* STORAGE_H */
