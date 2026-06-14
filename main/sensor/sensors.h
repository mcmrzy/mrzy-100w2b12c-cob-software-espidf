/* ================================================================
 *  sensors.h - 传感器读取接口
 * ================================================================ */

#ifndef SENSORS_H
#define SENSORS_H

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化传感器 */
esp_err_t sensors_init(void);

/* 读取电源数据 (电压、电流、功率) */
esp_err_t sensors_read_power(float *vbus, float *ibus, float *pbus, 
                              float *vbat, float *ibat, float *pbat);

/* 读取环境数据 (温度、气压) */
esp_err_t sensors_read_env(float *temp_led, float *temp_env, float *pressure);

#ifdef __cplusplus
}
#endif

#endif /* SENSORS_H */
