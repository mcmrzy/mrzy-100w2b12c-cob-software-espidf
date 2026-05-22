#include "bmp280.h"
#include "config.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "bmp280";

/* 全局数据实例 */
bmp280_data_t g_bmp280 = {0};

/* I2C 句柄和校准数据 */
static i2c_master_dev_handle_t i2c_dev = NULL;
static bmp280_calib_data_t calib_data;

/* 海平面标准气压 (hPa)，用于海拔估算 */
#define SEA_LEVEL_PRESSURE  1013.25f

/* ================================================================
 *  I2C 读写辅助函数
 * ================================================================ */
static esp_err_t bmp280_write_reg(uint8_t reg, uint8_t data)
{
    uint8_t buf[2] = {reg, data};
    return i2c_master_transmit(i2c_dev, buf, 2, 100);
}

static esp_err_t bmp280_read_reg(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(i2c_dev, &reg, 1, data, len, 100);
}

/* ================================================================
 *  读取校准参数
 * ================================================================ */
static bool bmp280_read_calibration(void)
{
    uint8_t calib[24];
    esp_err_t ret = bmp280_read_reg(BMP280_REG_CALIB, calib, 24);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read calibration data");
        return false;
    }

    calib_data.dig_T1 = (uint16_t)(calib[1] << 8) | calib[0];
    calib_data.dig_T2 = (int16_t)(calib[3] << 8) | calib[2];
    calib_data.dig_T3 = (int16_t)(calib[5] << 8) | calib[4];
    calib_data.dig_P1 = (uint16_t)(calib[7] << 8) | calib[6];
    calib_data.dig_P2 = (int16_t)(calib[9] << 8) | calib[8];
    calib_data.dig_P3 = (int16_t)(calib[11] << 8) | calib[10];
    calib_data.dig_P4 = (int16_t)(calib[13] << 8) | calib[12];
    calib_data.dig_P5 = (int16_t)(calib[15] << 8) | calib[14];
    calib_data.dig_P6 = (int16_t)(calib[17] << 8) | calib[16];
    calib_data.dig_P7 = (int16_t)(calib[19] << 8) | calib[18];
    calib_data.dig_P8 = (int16_t)(calib[21] << 8) | calib[20];
    calib_data.dig_P9 = (int16_t)(calib[23] << 8) | calib[22];

    ESP_LOGI(TAG, "Calibration data read OK");
    return true;
}

/* ================================================================
 *  温度补偿计算 (BMP280 官方算法)
 * ================================================================ */
static int32_t bmp280_compensate_temp(int32_t adc_T)
{
    int32_t var1, var2, T;
    var1 = ((((adc_T >> 3) - ((int32_t)calib_data.dig_T1 << 1))) * ((int32_t)calib_data.dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)calib_data.dig_T1)) * ((adc_T >> 4) - ((int32_t)calib_data.dig_T1))) >> 12) * ((int32_t)calib_data.dig_T3)) >> 14;
    calib_data.t_fine = var1 + var2;
    T = (calib_data.t_fine * 5 + 128) >> 8;
    return T;
}

/* ================================================================
 *  气压补偿计算 (BMP280 官方算法)
 * ================================================================ */
static uint32_t bmp280_compensate_press(int32_t adc_P)
{
    int64_t var1, var2, p;
    var1 = ((int64_t)calib_data.t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)calib_data.dig_P6;
    var2 = var2 + ((var1 * (int64_t)calib_data.dig_P5) << 17);
    var2 = var2 + (((int64_t)calib_data.dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)calib_data.dig_P3) >> 8) + ((var1 * (int64_t)calib_data.dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)calib_data.dig_P1) >> 33;
    if (var1 == 0) {
        return 0;
    }
    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)calib_data.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)calib_data.dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)calib_data.dig_P7) << 4);
    return (uint32_t)p;
}

/* ================================================================
 *  海拔估算
 * ================================================================ */
static float bmp280_calc_altitude(float pressure)
{
    // 简化公式: altitude = 44330 * (1 - (p/p0)^(1/5.255))
    return 44330.0f * (1.0f - powf(pressure / SEA_LEVEL_PRESSURE, 0.1903f));
}

/* ================================================================
 *  bmp280_init
 * ================================================================ */
bool bmp280_init(void)
{
    ESP_LOGI(TAG, "BMP280 init start (SCL=%d, SDA=%d, ADDR=0x%02X)",
             BMP280_SCL_PIN, BMP280_SDA_PIN, BMP280_ADDR);

    /* 创建 I2C 总线配置 */
    i2c_master_bus_config_t bus_cfg = {};
    bus_cfg.i2c_port = I2C_NUM_0;
    bus_cfg.sda_io_num = (gpio_num_t)BMP280_SDA_PIN;
    bus_cfg.scl_io_num = (gpio_num_t)BMP280_SCL_PIN;
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt = 7;
    bus_cfg.flags.enable_internal_pullup = true;

    i2c_master_bus_handle_t bus_handle;
    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus creation failed: %s", esp_err_to_name(ret));
        return false;
    }

    /* 添加 BMP280 设备 */
    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = BMP280_ADDR;
    dev_cfg.scl_speed_hz = 100000;  // 100kHz

    ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &i2c_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C device add failed: %s", esp_err_to_name(ret));
        return false;
    }

    /* 读取芯片 ID */
    uint8_t chip_id;
    ret = bmp280_read_reg(BMP280_REG_ID, &chip_id, 1);
    if (ret != ESP_OK || chip_id != 0x58) {
        ESP_LOGE(TAG, "BMP280 not found (ID=0x%02X, expected 0x58)", chip_id);
        return false;
    }
    ESP_LOGI(TAG, "BMP280 detected, ID=0x%02X", chip_id);

    /* 软复位 */
    bmp280_write_reg(BMP280_REG_RESET, 0xB6);
    vTaskDelay(pdMS_TO_TICKS(10));

    /* 读取校准参数 */
    if (!bmp280_read_calibration()) {
        return false;
    }

    /* 配置: 温度×2 + 气压×16 过采样, 正常模式 */
    // ctrl_meas: osrs_t=2 (×2), osrs_p=5 (×16), mode=3 (normal)
    bmp280_write_reg(BMP280_REG_CTRL, 0x57);

    /* 配置: IIR 滤波系数=4 */
    // config: t_sb=0 (0.5ms), filter=3 (×4), spi3w=0
    bmp280_write_reg(BMP280_REG_CONFIG, 0x18);

    ESP_LOGI(TAG, "BMP280 init done");
    return true;
}

/* ================================================================
 *  bmp280_read
 * ================================================================ */
bool bmp280_read(bmp280_data_t *data)
{
    if (i2c_dev == NULL || data == NULL) {
        return false;
    }

    uint8_t buf[6];
    esp_err_t ret = bmp280_read_reg(BMP280_REG_PRESS_MSB, buf, 6);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read sensor data");
        return false;
    }

    /* 解析原始数据 */
    int32_t adc_P = ((int32_t)buf[0] << 12) | ((int32_t)buf[1] << 4) | (buf[2] >> 4);
    int32_t adc_T = ((int32_t)buf[3] << 12) | ((int32_t)buf[4] << 4) | (buf[5] >> 4);

    /* 温度补偿 (必须先算温度，t_fine 用于气压补偿) */
    int32_t temp = bmp280_compensate_temp(adc_T);
    data->temperature = temp / 100.0f;  // 转换为摄氏度

    /* 气压补偿 */
    uint32_t press = bmp280_compensate_press(adc_P);
    data->pressure = press / 25600.0f;  // 转换为 hPa

    /* 海拔估算 */
    data->altitude = bmp280_calc_altitude(data->pressure);

    return true;
}
