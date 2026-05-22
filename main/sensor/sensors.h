#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct {
    float    bus_voltage;
    float    bat_voltage;
    float    temperature;       /* NTC 读取的灯珠温度 */
    uint16_t ldr_raw;
    uint16_t ldr_lux;
    float    current_bat;
    float    current_in;
    float    power_bat;
    float    power_in;
    /* BMP280 环境数据 */
    float    env_temperature;   /* 环境温度 (°C) */
    float    pressure;          /* 气压 (hPa) */
    float    altitude;          /* 海拔 (m) */
} SensorData;

extern SensorData g_sensors;
extern bool       g_auto_brightness;

void  sensors_init(void);
void  sensors_read(void);
float ntc_to_temp(uint16_t adc_val);

#ifdef __cplusplus
}
#endif
