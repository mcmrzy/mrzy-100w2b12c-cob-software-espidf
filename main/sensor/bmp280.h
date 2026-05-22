#ifndef BMP280_H
#define BMP280_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* BMP280 I2C 地址 */
#define BMP280_ADDR         0x76

/* 寄存器定义 */
#define BMP280_REG_ID       0xD0
#define BMP280_REG_RESET    0xE0
#define BMP280_REG_STATUS   0xF3
#define BMP280_REG_CTRL     0xF4
#define BMP280_REG_CONFIG   0xF5
#define BMP280_REG_PRESS_MSB 0xF7
#define BMP280_REG_TEMP_MSB  0xFA
#define BMP280_REG_CALIB     0x88

/* 校准参数结构体 */
typedef struct {
    uint16_t dig_T1;
    int16_t  dig_T2;
    int16_t  dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2;
    int16_t  dig_P3;
    int16_t  dig_P4;
    int16_t  dig_P5;
    int16_t  dig_P6;
    int16_t  dig_P7;
    int16_t  dig_P8;
    int16_t  dig_P9;
    int32_t t_fine;
} bmp280_calib_data_t;

/* BMP280 数据结构 */
typedef struct {
    float temperature;  // 摄氏度
    float pressure;     // hPa
    float altitude;     // 米 (估算)
} bmp280_data_t;

/* 全局数据 */
extern bmp280_data_t g_bmp280;

/* 函数声明 */
bool bmp280_init(void);
bool bmp280_read(bmp280_data_t *data);

#ifdef __cplusplus
}
#endif

#endif /* BMP280_H */
