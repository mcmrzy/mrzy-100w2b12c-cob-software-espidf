/**
 * @file config.h
 * @brief COB-LED 全局硬件配置和参数定义
 *
 * ESP-IDF v5.4 + ESP32-S3 项目配置头文件
 * 包含所有引脚定义、PWM参数、WiFi/MQTT/BLE配置、传感器参数等
 */

#ifndef CONFIG_H
#define CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ======================== 引脚定义 ======================== */

/* 使能 PWM（总亮度） */
#define PIN_EN_PWM          12

/* 暖白 PWM */
#define PIN_WPWM            10

/* 冷白 PWM */
#define PIN_YPWM            11

/* 风扇使能 */
#define PIN_FAN_EN          46

/* 风扇 PWM */
#define PIN_FAN_PWM         9

/* 编码器1（主编码器 - 色温/亮度） */
#define EC1_A               39
#define EC1_B               38
#define EC1_KEY             0

/* 编码器2（副编码器） */
#define EC2_A               37
#define EC2_B               36
#define EC2_KEY             35

/* 使能按键 */
#define PIN_EN_KEY          3

/* 蜂鸣器 */
#define PIN_BUZZER          13

/* ADC 通道 */
#define PIN_AD_BUS          6   /* 总线电压 */
#define PIN_AD_BAT          7   /* 电池电压 */
#define PIN_AD_LDR          5   /* 光敏电阻 */
#define PIN_AD_RT           4   /* NTC 温度 */
#define PIN_AD_I_BAT        1   /* 电池电流 */
#define PIN_AD_I_IN         2   /* 输入电流 */

/* ======================== LED 参数 ======================== */

#define EN_PWM_FREQ         20000   /* 使能 PWM 频率 (Hz) */
#define WY_PWM_FREQ         20000   /* 暖白/冷白 PWM 频率 (Hz) */
#define PWM_RESOLUTION      10      /* PWM 分辨率 (bit) */
#define PWM_MAX             1023    /* PWM 最大值 (2^10 - 1) */

#define CCT_MIN             2700    /* 最小色温 (K) */
#define CCT_MAX             6500    /* 最大色温 (K) */
#define CCT_STEP            100     /* 色温调节步进 (K) */
#define CCT_DEFAULT         4600    /* 默认色温 (K) */

#define BRT_MIN             0       /* 最小亮度 (%) */
#define BRT_MAX             100     /* 最大亮度 (%) */
#define BRT_DEFAULT         0       /* 默认亮度 (%) */

/* ======================== WiFi 配置 ======================== */

#define WIFI_AP_SSID        "COB-LED"
#define WIFI_AP_PASS        "12345678"
#define WIFI_HOSTNAME       "cob-led"

/* ======================== MQTT 配置 ======================== */

#define MQTT_BROKER         "192.168.4.1"
#define MQTT_PORT           1883
#define MQTT_TOPIC_SET      "cob-led/set"
#define MQTT_TOPIC_STATE    "cob-led/state"
#define MQTT_TOPIC_SENSOR   "cob-led/sensor"

/* ======================== BLE 配置 ======================== */

#define BLE_DEVICE_NAME     "COB-LED"

/* ======================== BMP280 配置 ======================== */

#define BMP280_SCL_PIN      8       /* I2C SCL */
#define BMP280_SDA_PIN      18      /* I2C SDA */
#define BMP280_ADDR         0x76    /* I2C 地址 (SDO 接地) */

/* ======================== 传感器参数 ======================== */

#define NTC_R_REF           10000.0f    /* NTC 参考电阻 (Ω) @ 25°C */
#define NTC_B               3435.0f     /* NTC B 值常数 */
#define NTC_T_REF           298.15f     /* NTC 参考温度 (K) = 25°C */

#define CURRENT_SENSE_RESISTOR  0.01f   /* 电流采样电阻 (Ω) */
#define CURRENT_GAIN        50.0f       /* 电流放大增益 */
#define CURRENT_SCALE_FACTOR   2.0f    /* 电流校准因子 */

#define LDR_DARK_THRESHOLD      500     /* 光敏暗阈值 */
#define LDR_BRIGHT_THRESHOLD    2500    /* 光敏亮阈值 */
#define AUTO_BRIGHT_INTERVAL    5000    /* 自动亮度调节间隔 (ms) */

/* ======================== 温度保护 ======================== */

#define TEMP_WARN           70      /* 温度警告阈值 (°C) */
#define TEMP_SHUTDOWN       85      /* 温度关断阈值 (°C) */

/* ======================== ADC 分压 ======================== */

#define ADC_BUS_SCALE       6.1f    /* 总线电压分压比 */
#define ADC_BAT_SCALE       5.1f    /* 电池电压分压比 */

/* ======================== 蜂鸣器 ======================== */

#define BUZZER_MIN_FREQ     200     /* 蜂鸣器最低频率 (Hz) */
#define BUZZER_MAX_FREQ     8000    /* 蜂鸣器最高频率 (Hz) */
#define BUZZER_MIN_DUTY     4       /* 蜂鸣器最小占空比 */
#define BUZZER_MAX_DUTY     128     /* 蜂鸣器最大占空比 */
#define BUZZER_DURATION     100     /* 蜂鸣器鸣叫时长 (ms) */

/* ======================== 风扇 ======================== */

#define FAN_PWM_FREQ        25000   /* 风扇 PWM 频率 (Hz) */
#define FAN_ON_TEMP         45      /* 风扇启动温度 (°C) */
#define FAN_FULL_TEMP       65      /* 风扇全速温度 (°C) */

/* ======================== 场景预设 ======================== */

#define SCENE_COUNT         4

static const uint16_t SCENES[SCENE_COUNT][2] = {
    {2700, 30},    /* 场景1: 暖光 2700K, 亮度 30% */
    {3500, 60},    /* 场景2: 中性暖 3500K, 亮度 60% */
    {4600, 80},    /* 场景3: 中性白 4600K, 亮度 80% */
    {6500, 100},   /* 场景4: 冷光 6500K, 亮度 100% */
};

/* ======================== 编码器 ======================== */

#define EC_STEPS_PER_DETENT    4       /* 编码器每格步数 */
#define ENCODER_DEBOUNCE_MS    5       /* 编码器消抖时间 (ms) */

/* ======================== ADC 采样 ======================== */

#define ADC_SAMPLES         10      /* ADC 多次采样取平均次数 */

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_H */
